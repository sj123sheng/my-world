import native from 'libnative_game.so';

interface InputEvent {
  type: number;
  x: number;
  y: number;
}

export interface Snapshot {
  hp: number;
  poise: number;
  x: number;
  y: number;
  fps: number;
  moving: boolean;
  targetDist: number;
  rendererReady: boolean;
}

export const nativeStart = (): void => native.nativeStart();
export const nativeStop = (): void => native.nativeStop();
export const pushInput = (e: InputEvent): void => native.pushInput(e);
export const pullSnapshot = (): Snapshot => native.pullSnapshot();
