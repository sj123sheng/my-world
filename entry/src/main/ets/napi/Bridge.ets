import native from 'libnative_game.so';

interface InputEvent {
  type: number;
  pointerId: number;
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
  moveX: number;
  moveY: number;
  cameraYaw: number;
  cameraPitch: number;
  targetDist: number;
  targetId: number;
  bossPhase: number;
  encounterMode: number;
  encounterState: number;
  rendererReady: boolean;
  stamina: number;
  comboSegment: number;
  invulnerable: boolean;
  insightMs: number;
  resonance: number;
  targetHp: number;
  targetPoise: number;
  pulseHitRemainingMs: number;
  lastRejectReason: number;
  currentAction: number;
  comboWindowMs: number;
  radianceCooldownMs: number;
  currentCooldownMs: number;
  corruptionCooldownMs: number;
  ultimateWindowMs: number;
  targetPoiseBroken: boolean;
  radianceAttached: boolean;
  currentAttached: boolean;
  corruptionAttached: boolean;
  corroded: boolean;
  currentReaction: number;
  pulsePhase: number;
  levelStage: number;
  gateState: number;
  supplyState: number;
  bossHp: number;
  bossPoise: number;
  bossMechanic: number;
  bossCastMs: number;
  perfLevel: number;
  vfxFlags: number;
  cameraShakeX: number;
  cameraShakeY: number;
  bossHpRatio: number;
  bossCastRatio: number;
  debugHud: boolean;
}

export const nativeStart = (): void => native.nativeStart();
export const nativeStop = (): void => native.nativeStop();
export const nativeStartIfForeground = (): void => native.nativeStartIfForeground();
export const nativeSetModelAssets = (player: ArrayBuffer, enemy: ArrayBuffer,
  boss: ArrayBuffer): boolean => native.nativeSetModelAssets(player, enemy, boss);
export const pushInput = (e: InputEvent): void => native.pushInput(e);
export const pushAction = (type: number): void => native.pushAction(type);
export const startEncounter = (mode: number): boolean => native.startEncounter(mode);
export const advanceLevel = (): boolean => native.advanceLevel();
export const useSupply = (): boolean => native.useSupply();
export const retryBoss = (): boolean => native.retryBoss();
export const toggleDebugHud = (): void => native.toggleDebugHud();
export const pullSnapshot = (): Snapshot => native.pullSnapshot();
