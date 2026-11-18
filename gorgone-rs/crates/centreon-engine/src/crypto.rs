//! Decryption of the `encrypt::` macro values, as `common/crypto/aes256.cc`
//! (and centreon-web's `Security\Encryption`): base64 of the IV (16 bytes),
//! an HMAC-SHA3-512 of the ciphertext (64 bytes) and the AES-256-CBC
//! ciphertext (PKCS#7). Keys from `engine-context.json`: `app_secret`
//! (base64, cut or padded with zeros to 32 bytes) and `salt` (base64, the
//! HMAC key). Nothing here logs a value.

use std::path::Path;

use aes::Aes256;
use aes::cipher::block_padding::Pkcs7;
use aes::cipher::{BlockDecryptMut, BlockEncryptMut, KeyIvInit};
use base64::Engine as _;
use base64::engine::general_purpose::STANDARD;
use hmac::{Hmac, Mac};
use sha3::Sha3_512;

const IV_LEN: usize = 16;
const MAC_LEN: usize = 64;

/// The keys of `engine-context.json`.
#[derive(Clone)]
pub struct Credentials {
    first: [u8; 32],
    second: Vec<u8>,
}

impl std::fmt::Debug for Credentials {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.write_str("Credentials(..)")
    }
}

fn mac(key: &[u8], data: &[u8]) -> Result<Hmac<Sha3_512>, String> {
    let mut mac =
        <Hmac<Sha3_512> as Mac>::new_from_slice(key).map_err(|_| "invalid HMAC key".to_owned())?;
    mac.update(data);
    Ok(mac)
}

impl Credentials {
    /// Keys of an `engine-context.json` document.
    pub fn from_json(bytes: &[u8]) -> Result<Self, String> {
        #[derive(serde::Deserialize)]
        struct File {
            app_secret: String,
            salt: String,
        }
        let file: File = serde_json::from_slice(bytes)
            .map_err(|_| "not an engine context document".to_owned())?;
        let first_key = STANDARD
            .decode(file.app_secret.trim())
            .map_err(|_| "app_secret is not base64".to_owned())?;
        let second = STANDARD
            .decode(file.salt.trim())
            .map_err(|_| "salt is not base64".to_owned())?;
        if second.is_empty() {
            return Err("empty salt".to_owned());
        }
        let mut first = [0u8; 32];
        let len = first_key.len().min(32);
        first[..len].copy_from_slice(&first_key[..len]);
        let credentials = Self { first, second };
        // As Engine: keys that cannot encrypt then decrypt are not used.
        let probe = credentials.encrypt_with_iv("test encrypt", [0; IV_LEN]);
        if credentials.decrypt(&probe).as_deref() != Ok("test encrypt") {
            return Err("these keys cannot encrypt and decrypt".to_owned());
        }
        Ok(credentials)
    }

    /// Keys of the file `path`; `Ok(None)` when it is absent or empty, as
    /// Engine ignores it then (`applier/state.cc`).
    pub fn from_file(path: &Path) -> Result<Option<Self>, String> {
        match std::fs::read(path) {
            Ok(bytes) if bytes.is_empty() => Ok(None),
            Ok(bytes) => Self::from_json(&bytes)
                .map(Some)
                .map_err(|error| format!("{}: {error}", path.display())),
            Err(error) if error.kind() == std::io::ErrorKind::NotFound => Ok(None),
            Err(error) => Err(format!("{}: {error}", path.display())),
        }
    }

    /// Encrypts with a given IV: the key self-test, and tests that need a
    /// value as centreon-web encrypts it. The engine never encrypts.
    pub fn encrypt_with_iv(&self, plain: &str, iv: [u8; IV_LEN]) -> String {
        let cipher = cbc::Encryptor::<Aes256>::new(&self.first.into(), &iv.into())
            .encrypt_padded_vec_mut::<Pkcs7>(plain.as_bytes());
        let tag = mac(&self.second, &cipher)
            .map(|m| m.finalize().into_bytes().to_vec())
            .unwrap_or_default();
        let mut mix = Vec::with_capacity(IV_LEN + MAC_LEN + cipher.len());
        mix.extend_from_slice(&iv);
        mix.extend_from_slice(&tag);
        mix.extend_from_slice(&cipher);
        STANDARD.encode(mix)
    }

    /// Decrypts a value (what follows `encrypt::`). The error never quotes
    /// the value.
    pub fn decrypt(&self, input: &str) -> Result<String, String> {
        let mix = STANDARD
            .decode(input.trim())
            .map_err(|_| "the content is not base64".to_owned())?;
        if mix.len() <= IV_LEN + MAC_LEN {
            return Err("the content is not AES256 encrypted".to_owned());
        }
        let (iv, rest) = mix.split_at(IV_LEN);
        let (tag, cipher) = rest.split_at(MAC_LEN);
        mac(&self.second, cipher)?
            .verify_slice(tag)
            .map_err(|_| "message authentication code validation failed".to_owned())?;
        let iv: [u8; IV_LEN] = iv.try_into().map_err(|_| "invalid IV".to_owned())?;
        let plain = cbc::Decryptor::<Aes256>::new(&self.first.into(), &iv.into())
            .decrypt_padded_vec_mut::<Pkcs7>(cipher)
            .map_err(|_| "decryption failed".to_owned())?;
        String::from_utf8(plain).map_err(|_| "decrypted value is not UTF-8".to_owned())
    }

    /// A macro value as Engine reads it once it has keys
    /// (`macros/grab_value.cc`): `raw::` removed, `encrypt::` decrypted.
    /// On a decryption error, the value stays as it is and `Err` says why.
    pub fn reveal(&self, value: &str) -> Result<Option<String>, String> {
        if let Some(raw) = value.strip_prefix("raw::") {
            Ok(Some(raw.to_owned()))
        } else if let Some(encrypted) = value.strip_prefix("encrypt::") {
            self.decrypt(encrypted).map(Some)
        } else {
            Ok(None)
        }
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use super::*;

    /// The bench key of `deploy/centreon/compose.yaml` (public on purpose).
    pub(crate) const BENCH_KEY: &str = r#"{"app_secret":"YmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmJiYmI=","salt":"c3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nzc3Nz"}"#;

    /// "s3cr3t-value" encrypted by centreon-web (`Security\Encryption`,
    /// PHP openssl) with BENCH_KEY.
    pub(crate) const FROM_WEB: &str = include_str!("../tests/data/encrypted-by-web.txt");

    #[test]
    fn decrypts_what_centreon_web_encrypts() {
        let keys = Credentials::from_json(BENCH_KEY.as_bytes()).unwrap();
        assert_eq!(keys.decrypt(FROM_WEB.trim()).unwrap(), "s3cr3t-value");
        assert_eq!(
            keys.reveal(&format!("encrypt::{}", FROM_WEB.trim()))
                .unwrap()
                .as_deref(),
            Some("s3cr3t-value")
        );
        assert_eq!(keys.reveal("raw::plain").unwrap().as_deref(), Some("plain"));
        assert_eq!(keys.reveal("plain").unwrap(), None);
        let round = keys.encrypt_with_iv("é ünïcode", [7; 16]);
        assert_eq!(keys.decrypt(&round).unwrap(), "é ünïcode");
    }

    #[test]
    fn refuses_what_it_cannot_authenticate() {
        let keys = Credentials::from_json(BENCH_KEY.as_bytes()).unwrap();
        let mut forged = STANDARD.decode(FROM_WEB.trim()).unwrap();
        let last = forged.len() - 1;
        forged[last] ^= 1;
        let error = keys.decrypt(&STANDARD.encode(forged)).unwrap_err();
        assert!(error.contains("authentication"), "{error}");
        assert!(!error.contains("s3cr3t"));
        assert!(keys.decrypt("not base64 !").is_err());
        assert!(keys.decrypt("c2hvcnQ=").is_err());
        let other = Credentials::from_json(
            br#"{"app_secret":"YWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWFhYWE=","salt":"dA=="}"#,
        )
        .unwrap();
        assert!(other.decrypt(FROM_WEB.trim()).is_err());
        assert!(Credentials::from_json(b"{}").is_err());
        assert!(format!("{keys:?}").contains(".."));
    }

    #[test]
    fn reads_the_key_file() {
        let dir = tempfile::tempdir().unwrap();
        let path = dir.path().join("engine-context.json");
        assert!(Credentials::from_file(&path).unwrap().is_none());
        std::fs::write(&path, "").unwrap();
        assert!(Credentials::from_file(&path).unwrap().is_none());
        std::fs::write(&path, BENCH_KEY).unwrap();
        assert!(Credentials::from_file(&path).unwrap().is_some());
        std::fs::write(&path, "{").unwrap();
        assert!(Credentials::from_file(&path).is_err());
    }
}
