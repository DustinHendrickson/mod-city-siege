-- ============================================================================
-- City Siege Module - Example Creature Text
-- ============================================================================
-- This is an example SQL file showing how to add custom text for siege creatures.
-- Replace CREATURE_ENTRY with the actual entry IDs of your siege creatures.
--
-- Usage:
-- 1. Choose or create creature entries for your siege forces
-- 2. Replace CREATURE_ENTRY placeholders with actual IDs
-- 3. Customize the text messages as desired
-- 4. Execute this SQL on your world database
-- ============================================================================

-- Example: Siege Leader Yells
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`)
VALUES
-- Initial spawn yells
(CREATURE_ENTRY, 0, 0, 'Your defenses will crumble before our might!', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Spawn Yell 1'),
(CREATURE_ENTRY, 0, 1, 'This city will burn!', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Spawn Yell 2'),
(CREATURE_ENTRY, 0, 2, 'Witness the fury of the Horde/Alliance!', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Spawn Yell 3'),

-- Combat yells
(CREATURE_ENTRY, 1, 0, 'Fall before me!', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Combat Yell 1'),
(CREATURE_ENTRY, 1, 1, 'Your leader will fall!', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Combat Yell 2'),
(CREATURE_ENTRY, 1, 2, 'None shall stand in our way!', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Combat Yell 3'),

-- Death yells
(CREATURE_ENTRY, 2, 0, 'This... is not... the end...', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Death Yell 1'),
(CREATURE_ENTRY, 2, 1, 'We will return... stronger...', 14, 0, 100, 0, 0, 0, 0, 0, 'Siege Leader - Death Yell 2');

-- Example: Mini-Boss Yells
INSERT INTO `creature_text` (`CreatureID`, `GroupID`, `ID`, `Text`, `Type`, `Language`, `Probability`, `Emote`, `Duration`, `Sound`, `BroadcastTextId`, `TextRange`, `comment`)
VALUES
(MINIBOSS_ENTRY, 0, 0, 'I will feast on your bones!', 14, 0, 100, 0, 0, 0, 0, 0, 'Mini-Boss - Spawn Yell'),
(MINIBOSS_ENTRY, 1, 0, 'Feel my wrath!', 14, 0, 100, 0, 0, 0, 0, 0, 'Mini-Boss - Combat Yell'),
(MINIBOSS_ENTRY, 2, 0, 'Impossible...', 14, 0, 100, 0, 0, 0, 0, 0, 'Mini-Boss - Death Yell');
