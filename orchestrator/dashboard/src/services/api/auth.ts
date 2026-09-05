import { request } from '@umijs/max';
import { getMyGames, type ProfileGame } from './me';

// Frontend session DTO aligned with the current Wingman dashboard auth flow.
export type SessionUser = {
  username: string;
  nickname?: string;
  roles: string[];
};

// Login response returned by the current dashboard auth endpoint.
export type SessionResponse = {
  token: string;
  user: SessionUser;
};

export async function createSession(params: {
  username: string;
  password: string;
}): Promise<SessionResponse> {
  return request<SessionResponse>('/api/v1/auth/login', {
    method: 'POST',
    data: params,
  });
}

// Compatibility projection for the current profile game-scope response shape.
export type CurrentUserGamesResponse = {
  games: ProfileGame[];
};

// Game scope loader used right after login to seed the scope store.
export async function fetchCurrentUserGames(): Promise<CurrentUserGamesResponse> {
  return getMyGames();
}
