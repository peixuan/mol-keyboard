export async function registerPwa(): Promise<ServiceWorkerRegistration | undefined> {
  if (!import.meta.env.PROD || !("serviceWorker" in navigator)) return undefined;
  try {
    const workerUrl = new URL("sw.js", document.baseURI);
    const registration = await navigator.serviceWorker.register(workerUrl, { scope: "./" });
    registration.addEventListener("updatefound", () => {
      const worker = registration.installing;
      worker?.addEventListener("statechange", () => {
        if (worker.state === "installed" && navigator.serviceWorker.controller !== null) {
          window.dispatchEvent(new CustomEvent("mol-pwa-update", { detail: registration }));
        }
      });
    });
    return registration;
  } catch (error: unknown) {
    window.dispatchEvent(new CustomEvent("mol-pwa-error", { detail: error }));
    return undefined;
  }
}
