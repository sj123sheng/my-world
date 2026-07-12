import native from 'libnative_game.so';

export const nativeStart = (id: string): void => native.nativeStart(id);
export const nativeStop = (): void => native.nativeStop();
export const pushInput = (e: {type: number, x: number, y: number}): void => native.pushInput(e);
