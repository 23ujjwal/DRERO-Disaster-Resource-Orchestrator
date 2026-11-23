export default function LastUpdated({ value }) {
  return <p className="last-updated text-muted" role="status">{value ? <>Last refreshed: <time dateTime={value.toISOString()}>{value.toLocaleTimeString()}</time></> : 'Waiting for data…'}</p>;
}
