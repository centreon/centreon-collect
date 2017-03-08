<?php
/*
 * Centreon
 *
 * Source Copyright 2005-2016 Centreon
 *
 * Unauthorized reproduction, copy and distribution
 * are not allowed.
 *
 * For more informations : contact@centreon.com
 *
 */
 
 /*
  *  Language management class
  */
 class CentreonBam_Lang
 {
 	protected $_charset;
 	protected $_lang;
 	protected $_path;
 	protected $_charsetList;

 	/*
 	 *  Constructor
 	 */
 	function __construct($centreon_path, $centreon = null)
 	{
		if (isset($centreon)) {
			$this->_lang = $centreon->user->lang;
			$this->_charset = $centreon->user->charset;
		} else {
			$this->_lang = "en_US";
			$this->_charset = "UTF-8";
		}
		$this->_path = $centreon_path;
		$this->setCharsetList();
 	}

 	/*
 	 *  Sets list of charsets
 	 */
 	protected function setCharsetList()
 	{
 		$this->_charsetList = array( "ISO-8859-1",
									"ISO-8859-2",
									"ISO-8859-3",
									"ISO-8859-4",
									"ISO-8859-5",
									"ISO-8859-6",
									"ISO-8859-7",
									"ISO-8859-8",
									"ISO-8859-9",
									"UTF-80",
									"UTF-83",
									"UTF-84",
									"UTF-85",
									"UTF-86",
									"ISO-2022-JP",
									"ISO-2022-KR",
									"ISO-2022-CN",
									"WINDOWS-1251",
									"CP866",
									"KOI8",
									"KOI8-E",
									"KOI8-R",
									"KOI8-U",
									"KOI8-RU",
									"ISO-10646-UCS-2",
									"ISO-10646-UCS-4",
									"UTF-7",
									"UTF-8",
									"UTF-16",
									"UTF-16BE",
									"UTF-16LE",
									"UTF-32",
									"UTF-32BE",
									"UTF-32LE",
									"EUC-CN",
									"EUC-GB",
									"EUC-JP",
									"EUC-KR",
									"EUC-TW",
									"GB2312",
									"ISO-10646-UCS-2",
									"ISO-10646-UCS-4",
									"SHIFT_JIS");
		sort($this->_charsetList);
 	}

 	/*
 	 *  Binds lang to the current Centreon page
 	 */
 	public function bindLang()
 	{
		putenv("LANG=$this->_lang");
		setlocale(LC_ALL, $this->_lang);
		bindtextdomain("messages", $this->_path."www/modules/centreon-bam-server/locale/");
		bind_textdomain_codeset("messages", $this->_charset);
		textdomain("messages");
 	}

 	/*
 	 *  Lang setter
 	 */
 	public function setLang($newLang)
 	{
 		$this->_lang = $newLang;
 	}

 	/*
 	 *  Returns lang that is being used
 	 */
 	public function getLang()
 	{
 		return $this->_lang;
 	}

 	/*
 	 *  Charset Setter
 	 */
 	public function setCharset($newCharset)
 	{
 		$this->_charset = $newCharset;
 	}

 	/*
 	 *  Returns charset that is being used
 	 */
 	public function getCharset()
 	{
 		return $this->_charset;
 	}

 	/*
 	 *  Returns an array with a list of charsets
 	 */
 	public function getCharsetList()
 	{
 		return $this->_charsetList;
 	}
}