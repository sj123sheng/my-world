export const nativeStart: () => void;
export const nativeStop: () => void;
export const pushInput: (event: {type: number, pointerId?: number, x: number, y: number}) => void;
export const pullSnapshot: () => {
  tick: number,
  hp: number,
  poise: number,
  x: number,
  y: number,
  fps: number,
  moving: boolean,
  targetDist: number,
  targetId: number,
  bossPhase: number,
  rendererReady: boolean
};
