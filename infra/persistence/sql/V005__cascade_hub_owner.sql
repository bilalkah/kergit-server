-- V005__cascade_hub_owner.sql
-- Change hub ownership deletion behavior:
-- When a user is deleted, delete their owned hubs (which cascades to members, channels, messages)

BEGIN;

-- First cleanup any existing orphan hubs
DELETE FROM public.hubs WHERE owner_id IS NULL;

-- Drop the existing foreign key constraint
ALTER TABLE public.hubs 
DROP CONSTRAINT IF EXISTS hubs_owner_id_fkey;

-- Re-add with ON DELETE CASCADE instead of ON DELETE SET NULL
ALTER TABLE public.hubs 
ADD CONSTRAINT hubs_owner_id_fkey 
FOREIGN KEY (owner_id) REFERENCES auth.users(id) ON DELETE CASCADE;

-- Make owner_id NOT NULL since we now require an owner
-- (optional - uncomment if you want to enforce this)
-- ALTER TABLE public.hubs ALTER COLUMN owner_id SET NOT NULL;

COMMIT;
