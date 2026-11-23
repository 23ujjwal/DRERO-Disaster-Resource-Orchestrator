import { useMemo, useState } from 'react';

// Neutralize spreadsheet formulas as well as quotes/newlines in exported cells.
function csvCell(value) {
  let text = value == null ? '' : typeof value === 'object' ? JSON.stringify(value) : String(value);
  if (/^[\s]*[=+\-@]/.test(text)) text = `'${text}`;
  return `"${text.replaceAll('"', '""')}"`;
}

export default function ResultTable({ columns, rows, filename = 'drero-results.csv', empty = 'No records returned.', caption }) {
  const [sort, setSort] = useState({ key: '', ascending: true });
  const sorted = useMemo(() => {
    if (!sort.key) return rows;
    return [...rows].sort((a, b) => {
      const x = a[sort.key], y = b[sort.key];
      const comparison = typeof x === 'number' && typeof y === 'number' ? x - y : String(x ?? '').localeCompare(String(y ?? ''), undefined, { numeric: true });
      return sort.ascending ? comparison : -comparison;
    });
  }, [rows, sort]);
  const exportCsv = () => {
    const exported = columns.filter(column => column.export !== false);
    const csv = [exported.map(column => column.label), ...sorted.map(row => exported.map(column => column.exportValue ? column.exportValue(row[column.key], row) : row[column.key]))].map(row => row.map(csvCell).join(',')).join('\r\n');
    const url = URL.createObjectURL(new Blob(['\uFEFF', csv], { type: 'text/csv;charset=utf-8' }));
    const anchor = document.createElement('a'); anchor.href = url; anchor.download = filename; anchor.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  };
  return (
    <div className="export-table">
      <div className="table-actions"><button type="button" className="btn btn-outline" disabled={!rows.length} onClick={exportCsv}>Export CSV</button><span className="text-muted">Select a column heading to sort.</span></div>
      <div className="table-responsive"><table className="table">
        {caption && <caption>{caption}</caption>}
        <thead><tr>{columns.map(column => <th key={column.key} aria-sort={sort.key === column.key ? (sort.ascending ? 'ascending' : 'descending') : 'none'}><button type="button" className="sort-heading" onClick={() => setSort(current => ({ key: column.key, ascending: current.key === column.key ? !current.ascending : true }))}>{column.label}{sort.key === column.key ? (sort.ascending ? ' ↑' : ' ↓') : ''}</button></th>)}</tr></thead>
        <tbody>{sorted.length ? sorted.map((row, index) => <tr key={row.request_id ?? row.audit_id ?? row.allocation_id ?? index}>{columns.map(column => <td key={column.key} data-metric={column.key}>{column.render ? column.render(row[column.key], row) : (row[column.key] ?? '—')}</td>)}</tr>) : <tr><td colSpan={columns.length}>{empty}</td></tr>}</tbody>
      </table></div>
    </div>
  );
}
