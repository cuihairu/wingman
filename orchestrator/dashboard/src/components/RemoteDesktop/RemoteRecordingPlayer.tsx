/**
 * 会话录像回放面板（公共件，设计 §16「回放」）。
 *
 * 受控 Modal：调用方（录像列表）给 name + load（取回录像 Blob 的动作）
 * 后，这里拉取 → SessionRecording 本地解析 → canvas 回放。播放/暂停/
 * 拖动进度全是浏览器本地行为（blob 解析 + canvas 绘制），不经网关、
 * 无注入面；取回录像的鉴权与审计在 recordings API（desktop:view，
 * desktop.recording_download）。
 *
 * 边界说明：不做变速播放——1.5.0 SessionRecording 没有变速 API，
 * 自行重排帧时序等于复刻解析器，不值得。
 */
import { useCallback, useEffect, useRef, useState } from 'react';
import { Alert, Button, Modal, Slider, Space, Spin, Typography } from 'antd';
import { CaretRightOutlined, PauseOutlined, ReloadOutlined } from '@ant-design/icons';
import { createRecordingPlayer, formatRecordingTime } from './recordingPlayer';
import type { RecordingPlayer } from './recordingPlayer';

const { Text } = Typography;

/** 回放画面舞台高度（px）；宽度随 Modal，画面按舞台等比缩放 */
const STAGE_HEIGHT = 460;

export interface RemoteRecordingPlayerProps {
  open: boolean;
  /** 录像文件名（标题与加载标识） */
  name: string;
  /** 取回录像内容的动作（服务层 fetchRecordingBlob 的注入点，便于复用/测试） */
  load: () => Promise<Blob>;
  onClose: () => void;
}

type Phase = 'loading' | 'ready' | 'error';

export default function RemoteRecordingPlayer({
  open,
  name,
  load,
  onClose,
}: RemoteRecordingPlayerProps) {
  const [phase, setPhase] = useState<Phase>('loading');
  const [errorMsg, setErrorMsg] = useState('');
  const [duration, setDuration] = useState(0);
  const [position, setPosition] = useState(0);
  const [playing, setPlaying] = useState(false);
  const [attempt, setAttempt] = useState(0);
  const stageRef = useRef<HTMLDivElement | null>(null);
  const playerRef = useRef<RecordingPlayer | null>(null);
  // load 走 ref：调用方常给内联箭头，进依赖会每次父渲染都重建播放器
  const loadRef = useRef(load);
  loadRef.current = load;

  /** 按舞台尺寸等比缩放画面（解析完成前分辨率未知，跳过） */
  const fitScale = useCallback(() => {
    const stage = stageRef.current;
    const player = playerRef.current;
    if (!stage || !player) {
      return;
    }
    const { width, height } = player.displaySize();
    if (width > 0 && stage.clientWidth > 0 && stage.clientHeight > 0) {
      player.scale(Math.min(stage.clientWidth / width, stage.clientHeight / height));
    }
  }, []);

  useEffect(() => {
    if (!open) {
      return undefined;
    }
    let disposed = false;
    setPhase('loading');
    setErrorMsg('');
    setDuration(0);
    setPosition(0);
    setPlaying(false);
    loadRef
      .current()
      .then((blob) => {
        if (disposed) {
          return;
        }
        const player = createRecordingPlayer(blob, {
          onReady: (ms) => {
            setDuration(ms);
            setPhase('ready');
            // 等一拍让 stage 先挂出画面再量尺寸（分辨率没落定时
            // onDisplayResize 会再触发一次 fitScale）
            requestAnimationFrame(() => fitScale());
          },
          onError: (message) => {
            setPhase('error');
            setErrorMsg(message);
          },
          onPosition: setPosition,
          onPlayingChange: setPlaying,
          onDisplayResize: () => fitScale(),
        });
        playerRef.current = player;
        if (stageRef.current) {
          stageRef.current.appendChild(player.element);
        }
      })
      .catch((e: unknown) => {
        if (disposed) {
          return;
        }
        setPhase('error');
        setErrorMsg(e instanceof Error ? e.message : '录像加载失败');
      });
    return () => {
      disposed = true;
      playerRef.current?.dispose();
      playerRef.current = null;
      if (stageRef.current) {
        stageRef.current.innerHTML = '';
      }
    };
  }, [open, name, attempt, fitScale]);

  useEffect(() => {
    if (phase !== 'ready') {
      return undefined;
    }
    const onResize = () => fitScale();
    window.addEventListener('resize', onResize);
    return () => window.removeEventListener('resize', onResize);
  }, [phase, fitScale]);

  const handleSeek = (value: number) => {
    playerRef.current?.seek(value);
    // seek 回调链经 onseek 回流；这里先手工置位避免拖动中回弹
    setPosition(value);
  };

  const renderStage = () => {
    if (phase === 'loading') {
      // Spin 的 tip 在无子内容时不渲染（antd v5 实测），文案独立成节点
      return (
        <div
          className="remote-recording-loading"
          style={{ padding: '150px 0', textAlign: 'center' }}
        >
          <Spin />
          <Typography.Text type="secondary" style={{ marginLeft: 12 }}>
            正在加载录像…
          </Typography.Text>
        </div>
      );
    }
    if (phase === 'error') {
      return (
        <Alert
          type="error"
          showIcon
          message="录像无法回放"
          description={errorMsg}
          action={
            <Button size="small" icon={<ReloadOutlined />} onClick={() => setAttempt((n) => n + 1)}>
              重试
            </Button>
          }
        />
      );
    }
    return null;
  };

  return (
    <Modal
      open={open}
      title={`回放：${name}`}
      width={960}
      destroyOnClose
      footer={
        <Button type="primary" onClick={onClose}>
          关闭
        </Button>
      }
      onCancel={onClose}
    >
      <div
        ref={stageRef}
        data-testid="remote-recording-stage"
        style={{
          height: STAGE_HEIGHT,
          background: '#000',
          borderRadius: 4,
          overflow: 'hidden',
          position: 'relative',
          display: phase === 'ready' ? undefined : 'none',
        }}
      />
      {renderStage()}
      <Space style={{ width: '100%', marginTop: 16 }} align="center">
        <Button
          type="primary"
          shape="circle"
          icon={playing ? <PauseOutlined /> : <CaretRightOutlined />}
          disabled={phase !== 'ready'}
          aria-label={playing ? '暂停' : '播放'}
          onClick={() => playerRef.current?.toggle()}
        />
        <Slider
          style={{ flex: 1, minWidth: 480, margin: '0 8px' }}
          min={0}
          max={duration || 1}
          step={100}
          value={position}
          disabled={phase !== 'ready'}
          tooltip={{ formatter: (v) => formatRecordingTime(v ?? 0) }}
          onChange={handleSeek}
        />
        <Text type="secondary" style={{ fontVariantNumeric: 'tabular-nums' }}>
          {formatRecordingTime(position)} / {formatRecordingTime(duration)}
        </Text>
      </Space>
    </Modal>
  );
}
