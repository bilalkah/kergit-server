// Browser-only: uses CDN ESM, no bundler required.
import { createClient } from 'https://esm.sh/@supabase/supabase-js@2';

let supabase = null;

export function initializeSupabase(url, anonKey) {
    if (!url || !anonKey) {
        throw new Error('Supabase URL and anon key are required');
    }
    supabase = createClient(url, anonKey);
    return supabase;
}

export async function signUp(email, password, userData = {}) {
    if (!supabase) throw new Error('Supabase not initialized');
    
    const { data, error } = await supabase.auth.signUp({
        email,
        password,
        options: {
            data: userData // This can include name, username, etc.
        }
    });
    
    if (error) throw error;
    
    const token = data?.session?.access_token;
    if (!token) throw new Error('No access_token from Supabase');
    
    return { token, user: data.user };
}

export async function loginWithPassword(email, password) {
    if (!supabase) throw new Error('Supabase not initialized');
    console.log('Logging in with email:', email);
    const { data, error } = await supabase.auth.signInWithPassword({ email, password });
    if (error) throw error;
    console.log('Login successful:', data);
    const token = data?.session?.access_token;
    if (!token) throw new Error('No access_token from Supabase');
    
    return { token, user: data.user };
}

export async function logout() {
    if (!supabase) return;
    await supabase.auth.signOut();
}

export function getCurrentUser() {
    if (!supabase) return null;
    return supabase.auth.getUser();
}

export function getSession() {
    if (!supabase) return null;
    return supabase.auth.getSession();
}

export function onAuthStateChange(callback) {
    if (!supabase) return null;
    return supabase.auth.onAuthStateChange(callback);
}