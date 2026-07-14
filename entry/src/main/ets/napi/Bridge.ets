import native from 'libnative_game.so';

interface InputEvent {
  type: number;
  pointerId?: number;
  x: number;
  y: number;
}

export interface Snapshot {
  tick: number;
  hp: number;
  poise: number;
  x: number;
  y: number;
  fps: number;
  moving: boolean;
  targetDist: number;
  targetId: number;
  bossPhase: number;
  rendererReady: boolean;
}

export const nativeStart = (): void => native.nativeStart();
export const nativeStop = (): void => native.nativeStop();
export const pushInput = (e: InputEvent): void => native.pushInput(e);
export const pullSnapshot = (): Snapshot => native.pullSnapshot();
