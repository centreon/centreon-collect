# Pipelane 
```mermaid
flowchart LR
    subgraph collect["Data collection"]
        TG["Telegraf<br/>(nagios plugins)"]
        CMA["Centreon<br/>Monitoring Agent"]
        POLL["Poller<br/>(active checks)"]
    end

    subgraph engine["Centreon Engine"]
        OTLIN["opentelemetry module<br/>(OTLP gRPC server :4317)"]
        CORE["Engine core<br/>check results"]
        CBMOD["cbmod"]
    end

    subgraph broker["Centreon Broker"]
        MUX["muxer"]
        USQL["20-unified_sql.so"]
        RRD["70-rrd.so"]
        OTLOUT["70-otlp.so<br/><b>NEW</b>"]
    end

    subgraph third["Third-party OTLP"]
        COL["OTel Collector "]
        PROM["Prometheus"]
        GRAF["Grafana"]
    end

    TG -->|OTLP| OTLIN
    CMA -->|OTLP| OTLIN
    POLL --> CORE
    OTLIN --> CORE
    CORE --> CBMOD
    CBMOD -->|BBDO| MUX
    MUX --> USQL
    MUX --> RRD
    MUX -->|pb_service_status<br/>pb_host_status| OTLOUT
    OTLOUT -->|<b>OTLP / gRPC</b>| COL
    COL --> PROM
    PROM --> GRAF

    style OTLOUT fill:#2d7d46,color:#fff
```



### Resource attributes

Emitted once per host, on the `ResourceMetrics.resource`.

| OTel attribute | Source | Status | Notes |
|---|---|---|---|
| `host.name` | `global_cache::get_host(host_id)->name()` | **available** | **The correlation key.** Centreon host name must equal the hostname CLM reports. |
| `service.name` | constant `"centreon-broker"` | **available** | The *emitting* service. See §5.3 — this is **not** the Centreon service. Becomes the Prometheus `job`. |
| `service.namespace` | constant `"centreon"` | available | |
| `service.instance.id` | `"<poller_name>:<broker_id>"` | derivable | Distinguishes broker instances |
| `service.version` | `CENTREON_BROKER_VERSION` | available | |
| `centreon.host.id` | `ServiceStatus.host_id` | available | Centreon-internal join key, kept for round-tripping |
| `centreon.poller.name` | poller/instance name | derivable | |
| `host.id` | — | **missing** | No machine-id/UUID in CIM today. See §8. |
| `host.arch` | — | **missing** | Not collected |
| `host.type` | — | **missing** | Not collected |
| `host.ip` | host `address` field | derivable | Centreon stores a resolution address, which may be a DNS name rather than an IP; needs validation before emitting |
| `os.type`, `os.name`, `os.version` | — | **missing** | Not collected |


# Data structure

```mermaid
flowchart LR
    subgraph alt["if the service were a resource attribute"]
        direction TB
        W1["ResourceMetrics host_1 and service_1"]
        W2["ResourceMetrics host_1 and service_2"]
        W3["ResourceMetrics host_1 and service_3"]
    end
    subgraph actual["what the module does"]
        direction TB
        C1["ResourceMetrics host_1"]
        C1 --> C2["datapoint centreon.service.id=1"]
        C1 --> C3["datapoint centreon.service.id=2"]
        C1 --> C4["datapoint centreon.service.id=3"]
    end
```

---

### start semcov : *

Centreon metrics:

| Centreon label | OTel metric | Static attribute |
|---|---|---|
| `cpu.user.percentage` | `system.cpu.utilization` | `cpu.mode=user` |
| `cpu.idle.percentage` | `system.cpu.utilization` | `cpu.mode=idle` |
| `memory.usage.bytes` | `system.memory.usage` | `system.memory.state=used` |
| `memory.free.bytes` | `system.memory.usage` | `system.memory.state=free` |
| `disk.io.read.bytes` | `system.disk.io` | `disk.io.direction=read` |

---

