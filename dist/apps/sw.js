// App Manager: never serve a stale file. Every same-origin GET made by the page
// (the page, its modules, the boot-logo and Doom helpers it imports, the app
// catalog and the .h9mod bundles) goes to the network and skips the browser's
// HTTP cache, so a normal reload is always enough. Falls back to the cache only
// when offline. Registered by app.mjs; this file is itself re-checked on every
// visit (updateViaCache: 'none').
self.addEventListener('install', () => self.skipWaiting());
self.addEventListener('activate', event => event.waitUntil(self.clients.claim()));
self.addEventListener('fetch', event => {
  const request = event.request;
  const url = new URL(request.url);
  if (request.method !== 'GET' || url.origin !== self.location.origin) return;
  event.respondWith(
    fetch(url.href, { cache: 'no-store', credentials: 'same-origin' })
      .catch(() => fetch(request)),   // offline: whatever the browser still has
  );
});
