import wsService from './websocket';

export interface ScreenshotData {
  image: string;
  width: number;
  height: number;
  timestamp: number;
}

type ScreenshotListener = (data: ScreenshotData) => void;

class ScreenshotService {
  private listeners: Set<ScreenshotListener> = new Set();
  private currentScreenshot: ScreenshotData | null = null;
  private subscribed = false;

  subscribe(listener: ScreenshotListener): () => void {
    this.listeners.add(listener);
    if (!this.subscribed) {
      this.subscribeToWebSocket();
      this.subscribed = true;
    }
    if (this.currentScreenshot) {
      listener(this.currentScreenshot);
    }
    return () => {
      this.listeners.delete(listener);
    };
  }

  getCurrent(): ScreenshotData | null {
    return this.currentScreenshot;
  }

  private subscribeToWebSocket() {
    wsService.on('screenshot', (msg) => {
      if (msg.data) {
        this.currentScreenshot = msg.data as ScreenshotData;
        this.notifyListeners();
      }
    });
  }

  private notifyListeners() {
    if (this.currentScreenshot) {
      this.listeners.forEach((listener) => {
        try {
          listener(this.currentScreenshot!);
        } catch (error) {
          console.error('[Screenshot] Listener error:', error);
        }
      });
    }
  }
}

const screenshotService = new ScreenshotService();
export default screenshotService;
