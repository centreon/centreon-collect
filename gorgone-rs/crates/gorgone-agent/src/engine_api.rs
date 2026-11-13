//! Client of the Centreon Engine gRPC API: external commands, configuration
//! reload and shutdown, for an agent that shares neither the command pipe nor
//! the service manager of the engine (sidecar container).

use std::future::Future;
use std::time::{Duration, SystemTime, UNIX_EPOCH};

use gorgone_proto::engine::engine_client::EngineClient;
use gorgone_proto::engine::engine_signal_process::Process;
use gorgone_proto::engine::{EngineSignalProcess, ExternalCommand};
use tonic::transport::{Channel, Endpoint};
use tonic::{Code, Request, Response, Status};

use crate::AgentError;
use crate::config::{EngineConfig, EngineRpc};

/// Handle on the engine API; clones share one connection.
#[derive(Clone)]
pub(crate) struct EngineApi {
    client: EngineClient<Channel>,
    /// Bound of one call, connection included.
    timeout: Duration,
}

impl EngineApi {
    /// Builds the client when `engine.grpc_url` is set. The channel connects
    /// on first use and reconnects by itself: an engine that is down fails
    /// the calls, not the agent.
    pub(crate) fn connect(config: &EngineConfig) -> Result<Option<Self>, AgentError> {
        let Some(url) = &config.grpc_url else {
            return Ok(None);
        };
        if !config.grpc_is_loopback() {
            tracing::warn!(
                grpc_url = %url,
                "engine.grpc_url is not a loopback address: engine commands travel in clear \
                 text, keep this link on a private network"
            );
        }
        let timeout = config.grpc_timeout();
        let endpoint = Endpoint::from_shared(url.clone())?.connect_timeout(timeout);
        Ok(Some(Self {
            client: EngineClient::new(endpoint.connect_lazy()),
            timeout,
        }))
    }

    /// Runs one call under the configured timeout; errors are ready to report.
    async fn call<T, R, F>(
        &self,
        message: T,
        rpc: impl FnOnce(Request<T>) -> F,
    ) -> Result<(), String>
    where
        F: Future<Output = Result<Response<R>, Status>>,
    {
        let mut request = Request::new(message);
        // Sent to the engine too, so that it drops a call nobody waits for.
        request.set_timeout(self.timeout);
        let silent = || {
            format!(
                "no answer from the engine API within {} ms",
                self.timeout.as_millis()
            )
        };
        match tokio::time::timeout(self.timeout, rpc(request)).await {
            Ok(Ok(_)) => Ok(()),
            // The same deadline, when the transport notices it first.
            Ok(Err(status))
                if matches!(status.code(), Code::Cancelled | Code::DeadlineExceeded) =>
            {
                Err(silent())
            }
            Ok(Err(status)) => Err(format!("{:?}: {}", status.code(), status.message())),
            Err(_) => Err(silent()),
        }
    }

    /// Sends one external command line, as it would be written to the pipe.
    pub(crate) async fn external_command(&self, line: &str) -> Result<(), String> {
        let mut client = self.client.clone();
        let command = ExternalCommand {
            command: line.to_owned(),
        };
        self.call(command, |request| async move {
            client.process_external_command(request).await
        })
        .await
    }

    /// Asks the engine to reload its configuration or to stop.
    pub(crate) async fn signal(&self, rpc: EngineRpc) -> Result<(), String> {
        let mut client = self.client.clone();
        match rpc {
            EngineRpc::Reload => {
                let now = SystemTime::now()
                    .duration_since(UNIX_EPOCH)
                    .map_or(0, |elapsed| elapsed.as_secs());
                // RESTART is the reload of the engine: same flag as SIGHUP.
                let signal = EngineSignalProcess {
                    process: Process::Restart.into(),
                    scheduled_time: u32::try_from(now).unwrap_or(u32::MAX),
                };
                self.call(signal, |request| async move {
                    client.signal_process(request).await
                })
                .await
            }
            EngineRpc::Shutdown => {
                self.call((), |request| async move {
                    client.shutdown_program(request).await
                })
                .await
            }
        }
    }
}
