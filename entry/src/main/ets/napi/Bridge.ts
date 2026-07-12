import native from 'libnative_game.so';

export const nativeStart = (id: string): void => native.nativeStart(id);
export const nativeStop = (): void => native.nativeStop();
