-- V001__init.sql
-- Requires: pgcrypto (for gen_random_uuid)
create extension if not exists pgcrypto;

begin;

-- ---------- helpers ----------
create or replace function public.set_updated_at()
returns trigger language plpgsql as $$
begin
  new.updated_at := now();
  return new;
end$$;

-- owner auto-membership (owner becomes member with role 'owner')
create or replace function public.ensure_owner_membership()
returns trigger language plpgsql as $$
begin
  if new.owner_id is not null then
    insert into public.hub_members(hub_id, user_id, role)
    values (new.id, new.owner_id, 'owner')
    on conflict (hub_id, user_id) do nothing;
  end if;
  return new;
end$$;

-- enforce: each user can own at most 2 hubs
create or replace function public.enforce_max_hubs_per_owner()
returns trigger language plpgsql as $$
declare cnt int;
begin
  if new.owner_id is null then
    return new;
  end if;
  select count(*) into cnt from public.hubs h
   where h.owner_id = new.owner_id;
  if tg_op = 'INSERT' and cnt >= 2 then
    raise exception 'Hub ownership limit reached (2 per user)';
  end if;
  return new;
end$$;

-- enforce: each hub can have at most 3 channels
create or replace function public.enforce_max_channels_per_hub()
returns trigger language plpgsql as $$
declare cnt int;
begin
  select count(*) into cnt from public.channels c
   where c.hub_id = new.hub_id;
  if tg_op = 'INSERT' and cnt >= 3 then
    raise exception 'Channel limit per hub reached (max 3)';
  end if;
  return new;
end$$;

-- ---------- tables ----------

-- hubs
create table if not exists public.hubs (
  id         uuid primary key default gen_random_uuid(),
  name       text not null,
  owner_id   uuid references auth.users(id) on delete set null,
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  constraint unique_owner_hub unique (owner_id, name)
);
create trigger trg_hubs_updated_at
  before update on public.hubs
  for each row execute function public.set_updated_at();
create trigger trg_hubs_owner_membership
  after insert on public.hubs
  for each row execute function public.ensure_owner_membership();
create trigger trg_hubs_limit
  before insert on public.hubs
  for each row execute function public.enforce_max_hubs_per_owner();

-- hub_members
create table if not exists public.hub_members (
  hub_id   uuid not null references public.hubs(id) on delete cascade,
  user_id  uuid not null references auth.users(id) on delete cascade,
  role     text not null check (role in ('owner','admin','member')),
  joined_at timestamptz not null default now(),
  primary key (hub_id, user_id)
);
create index if not exists idx_hub_members_user_id on public.hub_members(user_id);

-- channels
create table if not exists public.channels (
  id         uuid primary key default gen_random_uuid(),
  hub_id     uuid not null references public.hubs(id) on delete cascade,
  name       text not null,
  type       text not null check (type in ('text','voice')),
  created_at timestamptz not null default now(),
  updated_at timestamptz not null default now(),
  constraint unique_channel_per_hub unique (hub_id, name)
);
create index if not exists idx_channels_hub_id on public.channels(hub_id);
create trigger trg_channels_updated_at
  before update on public.channels
  for each row execute function public.set_updated_at();
create trigger trg_channels_limit
  before insert on public.channels
  for each row execute function public.enforce_max_channels_per_hub();

-- messages (text only; soft-delete via deleted_at)
create table if not exists public.messages (
  id          uuid primary key default gen_random_uuid(),
  channel_id  uuid not null references public.channels(id) on delete cascade,
  sender_id   uuid not null references auth.users(id)    on delete cascade,
  content     text not null,
  created_at  timestamptz not null default now(),
  updated_at  timestamptz not null default now(),
  deleted_at  timestamptz
);
create index if not exists idx_messages_channel_created_at
  on public.messages(channel_id, created_at desc);
create trigger trg_messages_updated_at
  before update on public.messages
  for each row execute function public.set_updated_at();

-- voice_state (ephemeral presence in voice channels)
create table if not exists public.voice_state (
  user_id    uuid not null references auth.users(id) on delete cascade,
  channel_id uuid not null references public.channels(id) on delete cascade,
  joined_at  timestamptz not null default now(),
  last_seen  timestamptz not null default now(),
  muted      boolean not null default false,
  deafened   boolean not null default false,
  primary key (user_id),              -- single voice channel at a time
  constraint voice_state_unique_in_hub unique (channel_id, user_id)
);

commit;
