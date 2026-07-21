export const nativeStart: () => void;
export const nativeStop: () => void;
export const nativeStartIfForeground: () => void;
export const nativeSetModelAssets: (player: ArrayBuffer, enemy: ArrayBuffer, boss: ArrayBuffer) => boolean;
export const pushInput: (event: {type: number, pointerId: number, x: number, y: number}) => void;
export const pushAction: (type: number) => void;
export const startEncounter: (mode: number) => boolean;
export const advanceLevel: () => boolean;
export const useSupply: () => boolean;
export const retryBoss: () => boolean;
export const toggleDebugHud: () => void;
export const pullSnapshot: () => {
  tick: number,
  hp: number,
  poise: number,
  x: number,
  y: number,
  fps: number,
  moving: boolean,
  moveX: number,
  moveY: number,
  cameraYaw: number,
  cameraPitch: number,
  targetDist: number,
  targetId: number,
  bossPhase: number,
  encounterMode: number,
  encounterState: number,
  rendererReady: boolean,
  stamina: number,
  comboSegment: number,
  invulnerable: boolean,
  insightMs: number,
  resonance: number,
  targetHp: number,
  targetPoise: number,
  pulseHitRemainingMs: number,
  lastRejectReason: number,
  currentAction: number,
  comboWindowMs: number,
  radianceCooldownMs: number,
  currentCooldownMs: number,
  corruptionCooldownMs: number,
  ultimateWindowMs: number,
  targetPoiseBroken: boolean,
  radianceAttached: boolean,
  currentAttached: boolean,
  corruptionAttached: boolean,
  corroded: boolean,
  currentReaction: number,
  pulsePhase: number,
  levelStage: number,
  gateState: number,
  supplyState: number,
  bossHp: number,
  bossPoise: number,
  bossMechanic: number,
  bossCastMs: number,
  perfLevel: number,
  vfxFlags: number,
  cameraShakeX: number,
  cameraShakeY: number,
  bossHpRatio: number,
  bossCastRatio: number,
  debugHud: boolean
};
