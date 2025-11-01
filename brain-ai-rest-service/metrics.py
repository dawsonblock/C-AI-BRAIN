"""
Prometheus-style metrics helper for the Brain-AI REST API.

Tracked series (by convention):
- Counter ``brain_ai_requests_total`` labelled by ``method`` and ``result`` capturing
  high-level request throughput.
- Histogram ``brain_ai_request_latency_seconds`` (same labels) storing raw latency
  samples for percentile export (keeps up to 1,024 recent samples per label by default
  while tracking full counts/sums; configurable via env/config).
- Various gauges populated by the C++ bindings, facts store, and other subsystems.
"""
import os
import time
import logging
from typing import Dict, Optional
from collections import defaultdict, deque
from datetime import datetime

logger = logging.getLogger(__name__)


def _resolve_histogram_max_samples() -> int:
    env_value = os.getenv("BRAIN_AI_METRICS_HIST_MAX_SAMPLES")
    if env_value is None:
        return 1024

    raw = str(env_value).strip()
    s = raw.lower().replace("_", "")
    try:
        if s.endswith("k"):
            base = s[:-1]
            if not base.isdigit():
                raise ValueError(f"invalid numeric part in '{raw}'")
            parsed = int(base) * 1000
        else:
            if not s.isdigit():
                raise ValueError(f"invalid numeric value '{raw}'")
            parsed = int(s)
        if parsed <= 0:
            raise ValueError("histogram max samples must be positive")
        return min(parsed, 100_000)
    except Exception as exc:  # pragma: no cover - defensive logging
        logger.warning(
            "Invalid BRAIN_AI_METRICS_HIST_MAX_SAMPLES=%s (%s); defaulting to 1024",
            raw,
            exc,
        )
        return 1024


class MetricsCollector:
    """Simple metrics collector for Prometheus-style metrics"""
    
    def __init__(self, max_hist_samples: Optional[int] = None):
        if max_hist_samples is None:
            max_hist_samples = _resolve_histogram_max_samples()
        elif max_hist_samples <= 0:
            logger.warning(
                "Received non-positive max_hist_samples=%s; defaulting to 1024",
                max_hist_samples,
            )
            max_hist_samples = 1024

        self.counters = defaultdict(int)
        self.gauges = defaultdict(float)
        self.histograms: Dict[str, deque] = {}
        self.histogram_stats = defaultdict(lambda: {"count": 0, "sum": 0.0})
        self.start_time = time.time()
        self._histogram_max_samples = max_hist_samples
    
    @property
    def histogram_max_samples(self) -> int:
        return self._histogram_max_samples

    def set_histogram_max_samples(self, max_samples: int) -> None:
        """Adjust the rolling window for histogram samples."""
        try:
            parsed = int(max_samples)
        except Exception as exc:
            raise ValueError(f"max_samples must be int-compatible: {exc}") from exc

        if parsed <= 0:
            raise ValueError("max_samples must be positive")

        parsed = min(parsed, 100_000)
        self._histogram_max_samples = parsed

        for key, existing in list(self.histograms.items()):
            self.histograms[key] = deque(existing, maxlen=parsed)
    
    def inc_counter(self, name: str, value: int = 1, labels: Optional[Dict] = None):
        """Increment a counter metric"""
        key = self._make_key(name, labels)
        self.counters[key] += value
    
    def set_gauge(self, name: str, value: float, labels: Optional[Dict] = None):
        """Set a gauge metric"""
        key = self._make_key(name, labels)
        self.gauges[key] = value
    
    def observe_histogram(self, name: str, value: float, labels: Optional[Dict] = None):
        """Observe a histogram metric"""
        key = self._make_key(name, labels)
        sample_value = float(value)
        if key not in self.histograms:
            self.histograms[key] = deque(maxlen=self._histogram_max_samples)
        self.histograms[key].append(sample_value)

        stats = self.histogram_stats[key]
        stats["count"] += 1
        stats["sum"] += sample_value
    
    def _make_key(self, name: str, labels: Optional[Dict]) -> str:
        """Create metric key with labels"""
        if not labels:
            return name
        label_str = ",".join(f'{k}="{v}"' for k, v in sorted(labels.items()))
        return f"{name}{{{label_str}}}"
    
    def _parse_key(self, key: str):
        """Parse metric key into name and labels"""
        if "{" not in key:
            return key, {}
        name, labels_str = key.split("{", 1)
        labels_str = labels_str.rstrip("}")
        labels = {}
        if labels_str:
            for pair in labels_str.split(","):
                k, v = pair.split("=")
                labels[k] = v.strip('"')
        return name, labels
    
    def export_prometheus(self) -> str:
        """Export metrics in Prometheus text format"""
        lines = []
        
        # Header
        lines.append("# Brain-AI REST API Metrics")
        lines.append(f"# Generated at {datetime.now().isoformat()}")
        lines.append("")
        
        # Uptime
        uptime = time.time() - self.start_time
        lines.append("# HELP brain_ai_uptime_seconds Time since service started")
        lines.append("# TYPE brain_ai_uptime_seconds gauge")
        lines.append(f"brain_ai_uptime_seconds {uptime:.2f}")
        lines.append("")
        
        # Counters
        if self.counters:
            lines.append("# Counters")
            for key, value in sorted(self.counters.items()):
                name, labels = self._parse_key(key)
                lines.append(f"# TYPE {name} counter")
                lines.append(f"{key} {value}")
            lines.append("")
        
        # Gauges
        if self.gauges:
            lines.append("# Gauges")
            for key, value in sorted(self.gauges.items()):
                name, labels = self._parse_key(key)
                lines.append(f"# TYPE {name} gauge")
                lines.append(f"{key} {value:.4f}")
            lines.append("")
        
        # Histograms (simplified - just p50, p95, p99)
        if self.histograms:
            lines.append("# Histograms")
            for key, samples in sorted(self.histograms.items()):
                name, labels = self._parse_key(key)
                stats = self.histogram_stats[key]
                if not samples:
                    continue

                sorted_values = sorted(samples)
                sample_count = len(sorted_values)

                lines.append(f"# TYPE {name} histogram")
                lines.append(f"{key}_count {stats['count']}")
                lines.append(f"{key}_sum {stats['sum']:.4f}")

                # Percentiles
                if sample_count > 0:
                    p50_idx = int((sample_count - 1) * 0.50)
                    p95_idx = int((sample_count - 1) * 0.95)
                    p99_idx = int((sample_count - 1) * 0.99)

                    lines.append(f"{key}_p50 {sorted_values[p50_idx]:.4f}")
                    lines.append(f"{key}_p95 {sorted_values[p95_idx]:.4f}")
                    lines.append(f"{key}_p99 {sorted_values[p99_idx]:.4f}")
            lines.append("")
        
        return "\n".join(lines)
    
    def get_summary(self) -> Dict:
        """Get metrics summary as dict"""
        summary = {
            "uptime_seconds": time.time() - self.start_time,
            "counters": dict(self.counters),
            "gauges": dict(self.gauges),
            "histograms": {}
        }
        
        for key, samples in self.histograms.items():
            if not samples:
                continue
            stats = self.histogram_stats[key]
            sorted_values = sorted(samples)
            sample_count = len(sorted_values)
            if sample_count > 0:
                p50_idx = min(int(sample_count * 0.50), sample_count - 1)
                p95_idx = min(int(sample_count * 0.95), sample_count - 1)
                p99_idx = min(int(sample_count * 0.99), sample_count - 1)
                p50 = sorted_values[p50_idx]
                p95 = sorted_values[p95_idx]
                p99 = sorted_values[p99_idx]
            else:
                p50 = p95 = p99 = 0

            summary["histograms"][key] = {
                "sample_count": sample_count,
                "total_count": stats["count"],
                "sum": stats["sum"],
                "p50": p50,
                "p95": p95,
                "p99": p99,
            }
        
        return summary


# Global metrics instance
metrics = MetricsCollector()

