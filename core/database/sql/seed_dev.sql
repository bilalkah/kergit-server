-- Users
INSERT INTO users (username, password_hash) VALUES
  ('alice', 'hash1'),
  ('bob',   'hash2')
ON CONFLICT (username) DO NOTHING;

-- Hub by Alice (owner auto-added to hub_members via trigger)
INSERT INTO hubs (name, owner_id) 
SELECT 'Alpha', id FROM users WHERE username = 'alice'
RETURNING id;

-- Get ids to use
WITH u AS (
  SELECT 
    (SELECT id FROM users WHERE username='alice') AS alice_id,
    (SELECT id FROM users WHERE username='bob')   AS bob_id,
    (SELECT h.id FROM hubs h JOIN users u ON u.id=h.owner_id WHERE u.username='alice' AND h.name='Alpha') AS alpha_id
),
c AS (
  -- Channels in Alpha
  INSERT INTO channels (hub_id, name, type)
  SELECT (SELECT alpha_id FROM u), 'general', 'text'
  UNION ALL
  SELECT (SELECT alpha_id FROM u), 'random',  'text'
  RETURNING id
)
SELECT 1;  -- noop to finish CTEs cleanly

-- Bob has NOT joined Alpha yet (so he should see nothing)
-- Alice is already a member (owner)
