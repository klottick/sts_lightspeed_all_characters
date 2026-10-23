(function (root) {
  'use strict';
  function text(tag, value, className) {
    const node = document.createElement(tag);
    node.textContent = value == null ? '' : String(value);
    if (className) node.className = className;
    return node;
  }
  function asset(report, key, alt, className) {
    const blobId = report?.assets?.byKey?.[key];
    const uri = blobId && report?.assets?.blobs?.[blobId];
    if (typeof uri === 'string' && /^data:image\/(png|jpeg|webp);base64,[A-Za-z0-9+/=]+$/.test(uri)) {
      const image = document.createElement('img'); image.src = uri; image.alt = alt || key || 'Artwork';
      if (className) image.className = className;
      return image;
    }
    const fallback = text('span', alt || key || 'Artwork unavailable', `asset-fallback${className ? ` ${className}` : ''}`);
    fallback.setAttribute('aria-label', alt || key || 'Artwork unavailable');
    return fallback;
  }
  function replaceChildren(parent, ...nodes) { parent.replaceChildren(...nodes.filter(Boolean)); return parent; }
  root.StsReportDom = { text, asset, replaceChildren };
}(globalThis));
