import { writable } from 'svelte/store';

export interface ScreenRegion {
	x: number;
	y: number;
	width: number;
	height: number;
}

export interface Screenshot {
	image: string;
	width: number;
	height: number;
	timestamp: number;
	region: ScreenRegion;
}

interface ScreenState {
	current: Screenshot | null;
	loading: boolean;
	error: string;
	lastUpdated: string;
}

const DEFAULT_REGION: ScreenRegion = { x: 0, y: 0, width: 1280, height: 720 };

function createDevScreenshot(region: ScreenRegion): Screenshot {
	const width = region.width > 0 ? region.width : DEFAULT_REGION.width;
	const height = region.height > 0 ? region.height : DEFAULT_REGION.height;
	const timestamp = Date.now();
	const svg = `<svg xmlns="http://www.w3.org/2000/svg" width="${width}" height="${height}" viewBox="0 0 ${width} ${height}">
		<defs>
			<linearGradient id="bg" x1="0" y1="0" x2="1" y2="1">
				<stop offset="0" stop-color="#0d1117"/>
				<stop offset="0.48" stop-color="#132238"/>
				<stop offset="1" stop-color="#0b3d2e"/>
			</linearGradient>
			<pattern id="grid" width="48" height="48" patternUnits="userSpaceOnUse">
				<path d="M 48 0 L 0 0 0 48" fill="none" stroke="#58a6ff" stroke-opacity="0.16" stroke-width="1"/>
			</pattern>
		</defs>
		<rect width="100%" height="100%" fill="url(#bg)"/>
		<rect width="100%" height="100%" fill="url(#grid)"/>
		<circle cx="${Math.round(width * 0.72)}" cy="${Math.round(height * 0.32)}" r="${Math.max(26, Math.round(Math.min(width, height) * 0.08))}" fill="#3fb950" fill-opacity="0.22"/>
		<rect x="${Math.round(width * 0.12)}" y="${Math.round(height * 0.18)}" width="${Math.round(width * 0.24)}" height="${Math.round(height * 0.18)}" rx="14" fill="#58a6ff" fill-opacity="0.16" stroke="#58a6ff" stroke-opacity="0.42"/>
		<rect x="${Math.round(width * 0.44)}" y="${Math.round(height * 0.58)}" width="${Math.round(width * 0.34)}" height="${Math.round(height * 0.16)}" rx="14" fill="#d29922" fill-opacity="0.18" stroke="#d29922" stroke-opacity="0.46"/>
		<text x="32" y="48" fill="#c9d1d9" font-family="Consolas, monospace" font-size="24">Wingman Preview</text>
		<text x="32" y="82" fill="#8b949e" font-family="Consolas, monospace" font-size="15">dev capture ${width}x${height} @ ${new Date(timestamp).toLocaleTimeString()}</text>
		<text x="32" y="${height - 36}" fill="#8b949e" font-family="Consolas, monospace" font-size="14">region ${region.x},${region.y},${region.width},${region.height}</text>
	</svg>`;

	return {
		image: `data:image/svg+xml;charset=utf-8,${encodeURIComponent(svg)}`,
		width,
		height,
		timestamp,
		region: { ...region, width, height },
	};
}

function createScreenStore() {
	const store = writable<ScreenState>({
		current: null,
		loading: false,
		error: '',
		lastUpdated: '-',
	});
	const invoke = (window as any).__TAURI_INVOKE__;

	return {
		subscribe: store.subscribe,
		async capture(region: ScreenRegion = DEFAULT_REGION) {
			store.update(state => ({ ...state, loading: true, error: '' }));
			try {
				const screenshot = invoke
					? await invoke('capture_screenshot', { region })
					: createDevScreenshot(region);
				store.set({
					current: screenshot,
					loading: false,
					error: '',
					lastUpdated: new Date(screenshot.timestamp || Date.now()).toLocaleTimeString(),
				});
				return screenshot as Screenshot;
			} catch (error: any) {
				const message = String(error?.message || error || '截图失败');
				store.update(state => ({ ...state, loading: false, error: message }));
				return null;
			}
		},
		clear() {
			store.set({ current: null, loading: false, error: '', lastUpdated: '-' });
		},
		loadDevData() {
			const screenshot = createDevScreenshot(DEFAULT_REGION);
			store.set({
				current: screenshot,
				loading: false,
				error: '',
				lastUpdated: new Date(screenshot.timestamp).toLocaleTimeString(),
			});
		},
	};
}

export const screen = createScreenStore();
export { DEFAULT_REGION };
