import { useIntl } from '@umijs/max';
import { Button } from 'antd';
import { getMessage, getNotification } from '@/utils/antdApp';
import defaultSettings from '../config/defaultSettings';

if (typeof window !== 'undefined' && !(window as any).WINGMAN_SERVER_ORIGIN) {
  const envOrigin = (process as any)?.env?.WINGMAN_SERVER_ORIGIN as string | undefined;
  if (envOrigin) {
    (window as any).WINGMAN_SERVER_ORIGIN = envOrigin;
  } else if (process.env.NODE_ENV === 'development') {
    (window as any).WINGMAN_SERVER_ORIGIN = 'http://127.0.0.1:9527';
  } else {
    (window as any).WINGMAN_SERVER_ORIGIN = window.location.origin;
  }
}

if (process.env.NODE_ENV === 'development' && typeof window !== 'undefined') {
  const origError = console.error?.bind(console);
  console.error = (...args: any[]) => {
    try {
      const message = args?.[0];
      if (typeof message === 'string' && message.includes('findDOMNode is deprecated')) {
        return;
      }
    } catch {}
    return origError?.(...args);
  };
}

const { pwa } = defaultSettings;
const isHttps = document.location.protocol === 'https:';

const clearCache = () => {
  if (window.caches) {
    caches
      .keys()
      .then((keys) => {
        keys.forEach((key) => {
          caches.delete(key);
        });
      })
      .catch((error) => console.log(error));
  }
};

if (pwa) {
  window.addEventListener('sw.offline', () => {
    getMessage()?.warning(useIntl().formatMessage({ id: 'app.pwa.offline' }));
  });

  window.addEventListener('sw.updated', (event: Event) => {
    const currentEvent = event as CustomEvent;
    const reloadSW = async () => {
      const worker = currentEvent.detail && currentEvent.detail.waiting;
      if (!worker) {
        return true;
      }

      await new Promise((resolve, reject) => {
        const channel = new MessageChannel();
        channel.port1.onmessage = (messageEvent) => {
          if (messageEvent.data.error) {
            reject(messageEvent.data.error);
          } else {
            resolve(messageEvent.data);
          }
        };
        worker.postMessage({ type: 'skip-waiting' }, [channel.port2]);
      });

      clearCache();
      window.location.reload();
      return true;
    };

    const key = `open${Date.now()}`;
    const button = (
      <Button
        type="primary"
        onClick={() => {
          getNotification()?.destroy(key);
          reloadSW();
        }}
      >
        {useIntl().formatMessage({ id: 'app.pwa.serviceworker.updated.ok' })}
      </Button>
    );

    getNotification()?.open({
      message: useIntl().formatMessage({ id: 'app.pwa.serviceworker.updated' }),
      description: useIntl().formatMessage({ id: 'app.pwa.serviceworker.updated.hint' }),
      btn: button,
      key,
      onClose: async () => null,
    });
  });
} else if ('serviceWorker' in navigator && isHttps) {
  const { serviceWorker } = navigator;
  if (serviceWorker.getRegistrations) {
    serviceWorker.getRegistrations().then((workers) => {
      workers.forEach((worker) => {
        worker.unregister();
      });
    });
  }

  serviceWorker.getRegistration().then((worker) => {
    if (worker) worker.unregister();
  });

  clearCache();
}
