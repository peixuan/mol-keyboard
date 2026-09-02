// SPDX-License-Identifier: Apache-2.0
"use strict";

const CACHE_NAME = "mol-keyboard-shell-v1";
const BASE_URL = new URL("./", self.location.href);
const FIXED_ASSETS = [
  "./",
  "./index.html",
  "./manifest.webmanifest",
  "./icons/mol-keyboard.svg",
  "./icons/mol-keyboard-maskable.svg",
  "./generated/mol_audio_worklet_core.js",
];

async function fetchChecked(url) {
  const response = await fetch(url, { cache: "reload" });
  if (!response.ok) throw new Error(`Could not cache ${url}: HTTP ${response.status}`);
  return response;
}

async function installShell() {
  const cache = await caches.open(CACHE_NAME);
  const indexUrl = new URL("./", BASE_URL);
  const indexResponse = await fetchChecked(indexUrl);
  const markup = await indexResponse.clone().text();
  await cache.put(indexUrl, indexResponse.clone());
  await cache.put(new URL("./index.html", BASE_URL), indexResponse);

  const discovered = [...markup.matchAll(/(?:src|href)="([^"#]+)"/gu)]
    .map((match) => match[1])
    .filter((path) => path !== undefined && !path.startsWith("data:"));
  const urls = new Set([...FIXED_ASSETS, ...discovered].map((path) => new URL(path, BASE_URL).href));
  urls.delete(indexUrl.href);
  urls.delete(new URL("./index.html", BASE_URL).href);
  await Promise.all(
    [...urls].map(async (url) => {
      const response = await fetchChecked(url);
      await cache.put(url, response);
    }),
  );
}

self.addEventListener("install", (event) => {
  event.waitUntil(installShell().then(() => self.skipWaiting()));
});

self.addEventListener("activate", (event) => {
  event.waitUntil(
    caches
      .keys()
      .then((names) => Promise.all(names.filter((name) => name !== CACHE_NAME).map((name) => caches.delete(name))))
      .then(() => self.clients.claim()),
  );
});

self.addEventListener("fetch", (event) => {
  const request = event.request;
  const url = new URL(request.url);
  if (request.method !== "GET" || url.origin !== self.location.origin) return;

  if (request.mode === "navigate") {
    event.respondWith(
      fetch(request)
        .then(async (response) => {
          if (response.ok) (await caches.open(CACHE_NAME)).put(request, response.clone());
          return response;
        })
        .catch(async () =>
          (await caches.match(request, { ignoreVary: true })) ??
          (await caches.match(new URL("./", BASE_URL), { ignoreVary: true })) ??
          Response.error(),
        ),
    );
    return;
  }

  event.respondWith(
    caches.match(request, { ignoreVary: true }).then(async (cached) => {
      if (cached !== undefined) return cached;
      const response = await fetch(request);
      if (response.ok) (await caches.open(CACHE_NAME)).put(request, response.clone());
      return response;
    }),
  );
});

self.addEventListener("message", (event) => {
  if (event.data?.type === "SKIP_WAITING") self.skipWaiting();
});
