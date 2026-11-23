export const schedulingMetrics = [
  { key: 'avg_turnaround_time', label: 'Average Turnaround', unit: 'simulated time units' },
  { key: 'avg_waiting_time', label: 'Average Waiting', unit: 'simulated time units' },
  { key: 'avg_response_time', label: 'Average Response', unit: 'simulated time units' },
  { key: 'throughput', label: 'Throughput', unit: 'requests per simulated time unit' },
  { key: 'starvation_count', label: 'Starvation', unit: 'requests waiting more than 20 simulated time units' },
  { key: 'requests_scheduled', label: 'Request Count', unit: 'requests' },
];

// Preserve the number supplied by the API, including zero and small throughput values.
export function formatSchedulingValue(value) {
  return typeof value === 'number' && Number.isFinite(value) ? String(value) : 'Unavailable';
}
