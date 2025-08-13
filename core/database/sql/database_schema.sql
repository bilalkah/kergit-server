CREATE USER chat_user WITH CREATEDB CREATEROLE PASSWORD '12345678' INHERIT;
DROP DATABASE IF EXISTS chat_db;
CREATE DATABASE chat_db OWNER chat_user;
\c chat_db

-- Users
CREATE TABLE users (
    id SERIAL PRIMARY KEY,
    username TEXT UNIQUE NOT NULL,
    password_hash TEXT NOT NULL,
    email TEXT,
    public_key TEXT, -- optional for E2EE later
    created_at TIMESTAMP DEFAULT NOW()
);

-- Hubs (invite-only)
CREATE TABLE hubs (
    id SERIAL PRIMARY KEY,
    name TEXT NOT NULL,
    owner_id INT REFERENCES users(id) ON DELETE CASCADE,
    created_at TIMESTAMP DEFAULT NOW()
);

-- Membership in hubs
CREATE TABLE hub_members (
    hub_id INT REFERENCES hubs(id) ON DELETE CASCADE,
    user_id INT REFERENCES users(id) ON DELETE CASCADE,
    role TEXT NOT NULL CHECK (role IN ('owner', 'admin', 'member')),
    PRIMARY KEY (hub_id, user_id)
);

-- Channels (all visible to hub members)
CREATE TABLE channels (
    id SERIAL PRIMARY KEY,
    hub_id INT REFERENCES hubs(id) ON DELETE CASCADE,
    name TEXT NOT NULL,
    type TEXT NOT NULL CHECK (type IN ('text', 'voice')),
    created_at TIMESTAMP DEFAULT NOW()
);

-- Messages (text only)
CREATE TABLE messages (
    id SERIAL PRIMARY KEY,
    channel_id INT REFERENCES channels(id) ON DELETE CASCADE,
    sender_id INT REFERENCES users(id) ON DELETE CASCADE,
    content TEXT NOT NULL, -- can be encrypted later
    sent_at TIMESTAMP DEFAULT NOW()
);
