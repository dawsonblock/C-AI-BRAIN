"""
Prometheus metrics for Brain-AI REST API
"""
import time
import logging
from typing import Dict, Optional
from collections import defaultdict
from datetime import datetime

logger = logging.getLogger(__name__)


class MetricsCollector:
    """Simple metrics collector for Prometheus-style metrics"""
    
    def __init__(self):
        self.counters = defaultdict(int)
        self.gauges = defaultdict(float)
        self.histograms = defaultdict(list)
        self.start_time = time.time()
    
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
        self.histograms[key].append(value)
    
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
            for key, values in sorted(self.histograms.items()):
                name, labels = self._parse_key(key)
                if not values:
                    continue
                
                sorted_values = sorted(values)
                count = len(sorted_values)
                total = sum(sorted_values)
                
                lines.append(f"# TYPE {name} histogram")
                lines.append(f"{key}_count {count}")
                lines.append(f"{key}_sum {total:.4f}")
                
                # Percentiles
                if count > 0:
                    p50_idx = int(count * 0.50)
                    p95_idx = int(count * 0.95)
                    p99_idx = int(count * 0.99)
                    
                    lines.append(f"{key}_p50 {sorted_values[min(p50_idx, count-1)]:.4f}")
                    lines.append(f"{key}_p95 {sorted_values[min(p95_idx, count-1)]:.4f}")
                    lines.append(f"{key}_p99 {sorted_values[min(p99_idx, count-1)]:.4f}")
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
        
        for key, values in self.histograms.items():
            if not values:
                continue
            sorted_values = sorted(values)
            count = len(sorted_values)
            summary["histograms"][key] = {
                "count": count,
                "sum": sum(sorted_values),
                "p50": sorted_values[int(count * 0.50)] if count > 0 else 0,
                "p95": sorted_values[int(count * 0.95)] if count > 0 else 0,
                "p99": sorted_values[int(count * 0.99)] if count > 0 else 0,
            }
        
        return summary


# Global metrics instance
metrics = MetricsCollector()

