//! Fake Centreon Engine gRPC API recording what the agent calls.

use std::net::SocketAddr;
use std::sync::{Arc, Mutex};
use std::time::Duration;

use gorgone_proto::engine::engine_server::{Engine, EngineServer};
use gorgone_proto::engine::engine_signal_process::Process;
use gorgone_proto::engine::{CommandSuccess, EngineSignalProcess, ExternalCommand};
use tokio_stream::wrappers::TcpListenerStream;
use tonic::transport::Server;
use tonic::{Request, Response, Status};

/// One call received by the fake engine.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum EngineCall {
    ExternalCommand(String),
    /// `SignalProcess` with its process and whether a schedule time was set.
    Signal(Process, bool),
    Shutdown,
}

#[derive(Default)]
struct State {
    calls: Mutex<Vec<EngineCall>>,
    /// External commands holding this text are refused.
    refused: Mutex<Option<String>>,
    /// Time every call takes to answer.
    delay: Mutex<Duration>,
}

struct Api(Arc<State>);

impl Api {
    async fn record(&self, call: EngineCall) {
        self.0.calls.lock().unwrap().push(call);
        let delay = *self.0.delay.lock().unwrap();
        tokio::time::sleep(delay).await;
    }
}

#[tonic::async_trait]
impl Engine for Api {
    async fn signal_process(
        &self,
        request: Request<EngineSignalProcess>,
    ) -> Result<Response<CommandSuccess>, Status> {
        let signal = request.into_inner();
        self.record(EngineCall::Signal(
            signal.process(),
            signal.scheduled_time > 0,
        ))
        .await;
        // The real engine leaves `value` unset here.
        Ok(Response::new(CommandSuccess { value: false }))
    }

    async fn shutdown_program(&self, _: Request<()>) -> Result<Response<()>, Status> {
        self.record(EngineCall::Shutdown).await;
        Ok(Response::new(()))
    }

    async fn process_external_command(
        &self,
        request: Request<ExternalCommand>,
    ) -> Result<Response<CommandSuccess>, Status> {
        let line = request.into_inner().command;
        self.record(EngineCall::ExternalCommand(line.clone())).await;
        let refused = self.0.refused.lock().unwrap().clone();
        if refused.is_some_and(|text| line.contains(&text)) {
            return Err(Status::invalid_argument(format!("unknown command {line}")));
        }
        Ok(Response::new(CommandSuccess { value: true }))
    }
}

/// Fake engine API listening on a local port.
pub struct FakeEngine {
    pub addr: SocketAddr,
    state: Arc<State>,
}

impl FakeEngine {
    pub async fn start() -> Self {
        let listener = tokio::net::TcpListener::bind("127.0.0.1:0").await.unwrap();
        let addr = listener.local_addr().unwrap();
        let state = Arc::new(State::default());
        let router = Server::builder().add_service(EngineServer::new(Api(state.clone())));
        tokio::spawn(router.serve_with_incoming(TcpListenerStream::new(listener)));
        Self { addr, state }
    }

    pub fn url(&self) -> String {
        format!("http://{}", self.addr)
    }

    /// Every call received so far, in arrival order.
    pub fn calls(&self) -> Vec<EngineCall> {
        self.state.calls.lock().unwrap().clone()
    }

    /// External command lines received so far, in arrival order.
    pub fn lines(&self) -> Vec<String> {
        self.calls()
            .into_iter()
            .filter_map(|call| match call {
                EngineCall::ExternalCommand(line) => Some(line),
                _ => None,
            })
            .collect()
    }

    pub fn refuse_commands_holding(&self, text: &str) {
        *self.state.refused.lock().unwrap() = Some(text.to_owned());
    }

    pub fn set_delay(&self, delay: Duration) {
        *self.state.delay.lock().unwrap() = delay;
    }
}
