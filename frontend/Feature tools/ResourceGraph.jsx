import { useId, useState } from 'react';
export default function ResourceGraph({ graph }) {
  const markerId = useId().replaceAll(':', '');
  const [selected, setSelected] = useState('');
  const [query, setQuery] = useState('');
  const [assignmentsOnly, setAssignmentsOnly] = useState(false);
  const [zoom, setZoom] = useState(100);
  if (!graph) return <p className="text-muted">No resource graph returned.</p>;
  const edges = graph.edges.filter(edge => !assignmentsOnly || edge.kind === 'assignment');
  const matches = new Set(graph.nodes.filter(node => node.label.toLowerCase().includes(query.toLowerCase())).map(node => node.id));
  const visible = new Set(matches);
  if (query) for (const edge of edges) if (matches.has(edge.from) || matches.has(edge.to)) { visible.add(edge.from); visible.add(edge.to); }
  const assignedNodes = new Set(edges.flatMap(edge => [edge.from, edge.to]));
  const nodes = graph.nodes.filter(node => visible.has(node.id) && (!assignmentsOnly || assignedNodes.has(node.id)));
  const requests = nodes.filter(node => node.kind === 'request'), resources = nodes.filter(node => node.kind === 'resource');
  const positions = new Map([...requests.map((node, i) => [node.id, { x: 20, y: 50 + i * 66 }]), ...resources.map((node, i) => [node.id, { x: 470, y: 50 + i * 66 }])]);
  const links = edges.filter(edge => positions.has(edge.from) && positions.has(edge.to));
  const selectedNode = graph.nodes.find(node => node.id === selected);
  return <section className="tool-panel resource-graph"><h3>Interactive resource allocation graph</h3>
    <p className="text-muted">{graph.scope}</p>
    <div className="filter-row"><input className="form-control" aria-label="Find a graph node" placeholder="Find request or resource" value={query} onChange={event => setQuery(event.target.value)} /><label><input type="checkbox" checked={assignmentsOnly} onChange={event => setAssignmentsOnly(event.target.checked)} /> Assignments only</label><label>Zoom <input aria-label="Graph zoom" type="range" min="70" max="180" value={zoom} onChange={event => setZoom(Number(event.target.value))} /></label></div>
    <p className="graph-legend"><span>● Request</span><span>■ Resource</span><span>Resource → request: assignment</span>{graph.edges.some(edge => edge.kind === 'request') && <span>Request → resource: explicit wait</span>}<span>Dashed → compatible candidate</span></p>
    {!nodes.length ? <p role="status">{assignmentsOnly && !query ? 'No active assignments.' : 'No matching graph nodes.'}</p> : <div className="graph-scroll"><svg style={{ width: `${zoom}%`, minWidth: 720 }} viewBox={`0 0 740 ${Math.max(requests.length, resources.length) * 66 + 75}`} role="group" aria-label="Resource allocation graph">
      <defs><marker id={markerId} viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M 0 0 L 10 5 L 0 10 z" fill="#94a3b8" /></marker></defs>
      <text x="20" y="24" fill="#e2e8f0">Requests</text><text x="470" y="24" fill="#e2e8f0">Resources</text>
      {links.map((edge, i) => { const a = positions.get(edge.from), b = positions.get(edge.to); const fromRequest = edge.from.startsWith('p:'); const highlight = selected === edge.from || selected === edge.to; return <line key={i} x1={a.x + (fromRequest ? 250 : 0)} y1={a.y + 22} x2={b.x + (fromRequest ? 0 : 250)} y2={b.y + 22} markerEnd={`url(#${markerId})`} stroke={highlight ? '#fbbf24' : '#64748b'} strokeWidth={highlight ? 3 : 1} opacity={!selected || highlight ? 1 : 0.2} strokeDasharray={edge.kind === 'candidate' ? '6 5' : undefined} />; })}
      {nodes.map(node => { const p = positions.get(node.id); return <g key={node.id} role="button" tabIndex="0" aria-pressed={selected === node.id} aria-label={node.label} onClick={() => setSelected(current => current === node.id ? '' : node.id)} onKeyDown={event => { if (event.key === 'Enter' || event.key === ' ') { event.preventDefault(); setSelected(current => current === node.id ? '' : node.id); } }}>
        <title>{node.label}</title><rect x={p.x} y={p.y} width="250" height="44" rx={node.kind === 'request' ? 22 : 4} fill={node.kind === 'request' ? '#1e3a8a' : '#064e3b'} stroke={selected === node.id ? '#fbbf24' : '#64748b'} strokeWidth="2" /><text x={p.x + 12} y={p.y + 27} fill="#f8fafc" fontSize="12">{node.label.length > 32 ? `${node.label.slice(0, 31)}…` : node.label}</text>
      </g>; })}
    </svg></div>}
    {selectedNode && <div role="status" className="graph-selection"><strong>{selectedNode.label}</strong><ul>{edges.filter(edge => edge.from === selected || edge.to === selected).map((edge, index) => <li key={index}>{graph.nodes.find(node => node.id === edge.from)?.label} → {graph.nodes.find(node => node.id === edge.to)?.label} ({edge.kind})</li>)}</ul></div>}
  </section>;
}
