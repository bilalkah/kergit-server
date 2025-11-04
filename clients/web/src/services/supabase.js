// src/services/supabase.js
// IMPORTANT: browser-friendly ESM import (no bundler needed)
import { createClient } from 'https://esm.sh/@supabase/supabase-js@2';

let supabase = null;

export function initSupabase(url, anon) {
  if (!url || !anon) throw new Error('[Supabase] Missing SUPABASE_URL or SUPABASE_ANON');
  supabase = createClient(url, anon);
  console.log('[Supabase] client initialized');
}

export async function loginWithPassword(email, password) {
  if (!supabase) throw new Error('[Supabase] client not initialized');
  const { data, error } = await supabase.auth.signInWithPassword({ email, password });
  if (error) throw new Error(`[Supabase] ${error.message}`);
  const { session, user } = data || {};
  if (!session?.access_token) throw new Error('[Supabase] no access token');
  console.log('[Supabase] login ok for', user?.email);
  return { token: session.access_token, user };
}

export async function logout() {
  if (!supabase) return;
  const { error } = await supabase.auth.signOut();
  if (error) throw new Error(`[Supabase] ${error.message}`);
  console.log('[Supabase] logout ok');
}
