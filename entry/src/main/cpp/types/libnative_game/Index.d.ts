export const nativeStart: () => void;
export const nativeStop: () => void;
export const pushInput: (event: {type: number, x: number, y: number}) => void;
export const pullSnapshot: () => {
  hp: number,
  poise: number,
  x: number,
  y: number,
  fps: number,
  moving: boolean,
  targetDist: number,
  rendererReady: boolean
};
