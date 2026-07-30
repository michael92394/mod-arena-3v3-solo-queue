/*
 * This file is part of the AzerothCore Project. See AUTHORS file for Copyright information
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Affero General Public License as published by the
 * Free Software Foundation; either version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "solo3v3_sc.h"
#include "PlayerGossip.h"
#include "PlayerGossipMgr.h"
#include "AccountMgr.h"
#include <unordered_map>
#include <unordered_set>

struct SoloMatchContext
{
    bool rated = false;
    uint32 teamMMR[BG_TEAMS_COUNT] = { 1500, 1500 };
    std::unordered_set<uint32> penalizedPlayers;
    std::unordered_set<uint32> rewardedPlayers;
};

static std::unordered_map<uint32, SoloMatchContext> g_soloMatchContexts;

void NpcSolo3v3::Initialize()
{
    for (int i = 0; i < MAX_TALENT_CAT; i++)
        cache3v3Queue[i] = 0;

    lastFetchQueueList = 0;
}

bool NpcSolo3v3::OnGossipHello(Player* player, Creature* creature)
{
    if (!player)
        return true;

    if (sConfigMgr->GetOption<bool>("Solo.3v3.Enable", true) == false)
    {
        ChatHandler(player->GetSession()).SendSysMessage("Arena disabled!");
        return true;
    }

    fetchQueueList();
    std::stringstream infoQueue;

    infoQueue << " ---------------------------------------------";
    infoQueue << "\n               " << (cache3v3Queue[MELEE] + cache3v3Queue[RANGE] + cache3v3Queue[HEALER]) << " Queued Player(s)";
    infoQueue << "\n                 |TInterface/ICONS/ability_rogue_shadowstrikes:21:21:0:11|t       |TInterface/ICONS/spell_shadow_shadowembrace:21:21:0:11|t        |TInterface/ICONS/spell_holy_holynova:21:21:0:11|t";
    infoQueue << "\n\n              Melee  Caster  Healer";
    infoQueue << "\n                 [" << cache3v3Queue[MELEE] << "]        [" << cache3v3Queue[RANGE] << "]        [" << cache3v3Queue[HEALER] << "]";
    infoQueue << "\n\n   |TInterface\\icons\\inv_jewelry_talisman_04:17:17:0:30|t [" << cache3v3Queue[SHAMAN] << "]  " << " |TInterface\\icons\\inv_hammer_01:17:17:0:30|t [" << cache3v3Queue[PALADIN] << "]  "
        << " |TInterface\\icons\\inv_sword_27:17:17:0:30|t [" << cache3v3Queue[WARRIOR] << "]  " << " |TInterface\\icons\\inv_misc_monsterclaw_04:17:17:0:30|t [" << cache3v3Queue[DRUID] << "]  "
        << " |TInterface\\icons\\spell_deathknight_classicon:17:17:0:30|t [" << cache3v3Queue[DK] << "]" << "\n   |TInterface\\icons\\spell_nature_drowsy:17:17:0:30|t [" << cache3v3Queue[WARLOCK] << "]  "
        << " |TInterface\\icons\\inv_staff_30:17:17:0:30|t [" << cache3v3Queue[PRIEST] << "]  " << " |TInterface\\icons\\inv_weapon_bow_07:17:17:0:30|t [" << cache3v3Queue[HUNTER] << "]  "
        << " |TInterface\\icons\\inv_staff_13:17:17:0:30|t [" << cache3v3Queue[MAGE] << "]  " << " |TInterface\\icons\\inv_throwingknife_04:17:17:0:30|t [" << cache3v3Queue[ROGUE] << "]";
    AddGossipItemFor(player, GOSSIP_ICON_CHAT, infoQueue.str().c_str(), GOSSIP_SENDER_MAIN, 0);

        bool inSoloQueue = player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO);
    bool inNormal3v3 = player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3);

    if (inSoloQueue || inNormal3v3)
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_2v2_7:30:30:-18:0|t Leave Arena queue", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_LEAVE_QUEUE, "Are you sure you want to leave the arena queue?", 0, false);

    if (!inSoloQueue && !inNormal3v3)
		AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_3v3_5:30:30:-18:0|t Queue Solo 3v3 (Skirmish)", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_UNRATED);

	bool ratedEnabled = sSolo->IsRatedEnabled();
	if (ratedEnabled && !inSoloQueue && !inNormal3v3)
		AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_3v3_5:30:30:-18:0|t Queue 3v3soloQ (Rated)", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED);

    // Solo Queue uses a separate ladder table and does NOT require a permanent ArenaTeam.
    // Keep the NPC UI focused on queueing + stats (no create/disband team).
    AddGossipItemFor(player, GOSSIP_ICON_DOT, "|TInterface/ICONS/INV_Misc_Coin_01:30:30:-18:0|t Show statistics", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_GET_STATISTICS);

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/inv_misc_questionmark:30:30:-20:0|t Help", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_SCRIPT_INFO);

    SendGossipMenuFor(player, 60015, creature ? creature->GetGUID() : player->GetGUID());

    return true;
}

bool NpcSolo3v3::OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
    if (!player)
        return true;

    player->PlayerTalkClass->ClearMenus();

    switch (action)
    {
        case NPC_3v3_ACTION_CREATE_ARENA_TEAM:
{
    // Solo Queue does not require a permanent ArenaTeam (separate ladder table).
    ChatHandler(player->GetSession()).SendSysMessage("Solo Queue does not require an arena team. You can queue immediately.");
    CloseGossipMenuFor(player);
    return true;
}

        case NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED:
        {
            if (!sSolo->IsRatedEnabled())
            {
                ChatHandler(player->GetSession()).SendSysMessage("Rated Solo 3v3 is currently disabled.");
                CloseGossipMenuFor(player);
                return true;
            }

            // check Deserter debuff
            if (player->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
            {
                WorldPacket data;
                sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
                player->GetSession()->SendPacket(&data);
            }
            else
                if (ArenaCheckFullEquipAndTalents(player) && JoinQueueArena(player, creature, true) == false)
                    ChatHandler(player->GetSession()).SendSysMessage("Something went wrong while joining queue. Already in another queue?");

            CloseGossipMenuFor(player);
            return true;
        }

        case NPC_3v3_ACTION_JOIN_QUEUE_ARENA_UNRATED:
        {
            // check Deserter debuff
            if (player->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
            {
                WorldPacket data;
                sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
                player->GetSession()->SendPacket(&data);
            }
            else
                if (ArenaCheckFullEquipAndTalents(player) && JoinQueueArena(player, creature, false) == false)
                    ChatHandler(player->GetSession()).SendSysMessage("Something went wrong while joining queue. Already in another queue?");

            CloseGossipMenuFor(player);
            return true;
        }

        case NPC_3v3_ACTION_LEAVE_QUEUE:
        {
            if (player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO) ||
                player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3))
            {
                uint8 arenaType = 3; // 3v3 display
                WorldPacket Data;
                Data << arenaType << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
                player->GetSession()->HandleBattleFieldPortOpcode(Data);
                CloseGossipMenuFor(player);
            }
            return true;
        }

        case NPC_3v3_ACTION_GET_STATISTICS:
{
    // Show SoloQ ladder stats from the separate ladder table (no ArenaTeam required).
    uint32 rating = 0;
    uint32 mmr = 0;
    Solo3v3::instance()->GetSoloRatingAndMMR(player, rating, mmr);

    std::stringstream s;
    s << "Solo 3v3 Rating: " << rating;
    s << "\nSolo 3v3 MMR: " << mmr;

    ChatHandler(player->GetSession()).PSendSysMessage("{}", s.str().c_str());
    CloseGossipMenuFor(player);
    return true;
}

        case NPC_3v3_ACTION_DISBAND_ARENATEAM:
{
    // Legacy button kept for compatibility in some forks, but Solo Queue does not use ArenaTeam.
    ChatHandler(player->GetSession()).SendSysMessage("Solo Queue does not use arena teams. Nothing to disband.");
    CloseGossipMenuFor(player);
    return true;
}
        break;

        case NPC_3v3_ACTION_SCRIPT_INFO:
        {

            AddGossipItemFor(player, GOSSIP_ICON_CHAT, "<- Back", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_MAIN_MENU);
            SendGossipMenuFor(player, NPC_TEXT_3v3, creature ? creature->GetGUID() : player->GetGUID());
            return true;
        }
        break;

        case NPC_3v3_ACTION_MAIN_MENU:
        {
            OnGossipHello(player, creature);
            return true;
        }

    }

    OnGossipHello(player, creature);
    return true;
}

bool NpcSolo3v3::ArenaCheckFullEquipAndTalents(Player* player)
{
    if (!player)
        return false;

    if (!sConfigMgr->GetOption<bool>("Arena.CheckEquipAndTalents", true))
        return true;

    std::stringstream err;

    if (player->GetFreeTalentPoints() > 0)
        err << "You have currently " << player->GetFreeTalentPoints() << " free talent points. Please spend all your talent points before queueing arena.\n";

    Item* newItem = NULL;
    for (uint8 slot = EQUIPMENT_SLOT_START; slot < EQUIPMENT_SLOT_END; ++slot)
    {
        if (slot == EQUIPMENT_SLOT_OFFHAND || slot == EQUIPMENT_SLOT_RANGED || slot == EQUIPMENT_SLOT_TABARD || slot == EQUIPMENT_SLOT_BODY)
            continue;

        newItem = player->GetItemByPos(INVENTORY_SLOT_BAG_0, slot);
        if (newItem == NULL)
        {
            err << "Your character is not fully equipped.\n";
            break;
        }
    }

    if (err.str().length() > 0)
    {
        ChatHandler(player->GetSession()).SendSysMessage(err.str().c_str());
        return false;
    }

    return true;
}

bool NpcSolo3v3::JoinQueueArena(Player* player, Creature* /*creature*/, bool isRated)
{
    if (!player)
        return false;

    // RTG note: keep default MinLevel low so level-locked realms (like 19) work out of the box.
    if (sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 19) > player->GetLevel())
        return false;

    // Rated: require the standalone schema and block playerbots / rndbot accounts.
    if (isRated)
    {
        if (!sSolo->IsRatedEnabled())
            return false;

        std::string botPrefix = sConfigMgr->GetOption<std::string>("AiPlayerbot.RandomBotAccountPrefix", "rndbot");
        if (!botPrefix.empty())
        {
            std::string accName;
            if (AccountMgr::GetName(player->GetSession()->GetAccountId(), accName))
            {
                if (accName.rfind(botPrefix, 0) == 0) // starts_with
                    return false;
            }
        }
    }

    uint8 displayArenaType = 3; // force 3v3 display in client status packet
    uint32 arenaRating = 0;
    uint32 matchmakerRating = 0;

    // Unrated should use the normal 3v3 skirmish bucket so it can pop with standard 3v3 queuers (incl. bots).
    // Rated keeps using the Solo queue bucket/layering used by this module.
    BattlegroundQueueTypeId queueTypeId = isRated ? bgQueueTypeId : (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3;
    uint8 queueArenaType = isRated ? uint8(ARENA_TYPE_3v3_SOLO) : uint8(ARENA_TYPE_3v3);

    // ignore if we already in BG, Arena or any arena queue
    if (player->InBattleground() || player->InArena() ||
        player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_2v2) ||
        player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3) ||
        player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_5v5) ||
        player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO) ||
        player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_1v1))
        return false;

    //check existance
    Battleground* bg = sBattlegroundMgr->GetBattlegroundTemplate(BATTLEGROUND_AA);

    if (!bg)
    {
        LOG_ERROR("module", "Battleground: template bg (all arenas) not found");
        return false;
    }

    if (DisableMgr::IsDisabledFor(DISABLE_TYPE_BATTLEGROUND, BATTLEGROUND_AA, nullptr))
    {
        ChatHandler(player->GetSession()).PSendSysMessage(LANG_ARENA_DISABLED);
        return false;
    }

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketByLevel(bg->GetMapId(), player->GetLevel());
    if (!bracketEntry)
        return false;

    // Only reject when the player is already in this exact queue.
    // GetBattlegroundQueueIndex returns PLAYER_MAX_BATTLEGROUND_QUEUES when the queue is not present,
    // so the old "< PLAYER_MAX_BATTLEGROUND_QUEUES" check inverted the meaning and blocked fresh joins.
    if (player->GetBattlegroundQueueIndex(queueTypeId) >= PLAYER_MAX_BATTLEGROUND_QUEUES && !player->HasFreeBattlegroundQueueId())
        return false;

    uint32 ateamId = 0;

    if (isRated)
    {
        // Rated SoloQ uses its own ladder table and does NOT require a permanent ArenaTeam.
        uint32 rating = 0;
        uint32 mmr = 0;
        Solo3v3::instance()->GetSoloRatingAndMMR(player, rating, mmr);
        arenaRating = rating;
        matchmakerRating = mmr;
        ateamId = 0;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(queueTypeId);
    BattlegroundTypeId bgTypeId = BATTLEGROUND_AA;

    bg->SetRated(isRated);
    bg->SetMinPlayersPerTeam(3);

    GroupQueueInfo* ginfo = bgQueue.AddGroup(player, nullptr, bgTypeId, bracketEntry, displayArenaType, isRated, false, arenaRating, matchmakerRating, ateamId, 0);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo);
    uint32 queueSlot = player->AddBattlegroundQueueId(queueTypeId);

    // send status packet (in queue)
    WorldPacket data;
    sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, displayArenaType, TEAM_NEUTRAL, isRated);
    player->GetSession()->SendPacket(&data);

    if (isRated && matchmakerRating == 0)
        matchmakerRating = 1;

    sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, queueArenaType, queueTypeId, bgTypeId, bracketEntry->GetBracketId());
    sScriptMgr->OnPlayerJoinArena(player);

    return true;
}

bool NpcSolo3v3::CreateArenateam(Player* player, Creature* /*creature*/)
{
    if (player)
        ChatHandler(player->GetSession()).SendSysMessage("Solo Queue uses a standalone ladder and does not create permanent arena teams.");

    return false;
}

void NpcSolo3v3::fetchQueueList()
{
    if (GetMSTimeDiffToNow(lastFetchQueueList) < 1000)
        return;

    lastFetchQueueList = getMSTime();

    BattlegroundQueue* queue = &sBattlegroundMgr->GetBattlegroundQueue((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO);

    for (int i = 0; i < MAX_TALENT_CAT; i++)
        cache3v3Queue[i] = 0;

    for (int i = BG_BRACKET_ID_FIRST; i <= BG_BRACKET_ID_LAST; i++)
    {
        for (int j = 0; j < 2; j++)
        {
            for (auto queueInfo : queue->m_QueuedGroups[i][j])
            {
                if (queueInfo->IsInvitedToBGInstanceGUID) // Skip when invited
                    continue;

                for (auto const& playerGuid : queueInfo->Players)
                {
                    Player* _player = ObjectAccessor::FindPlayer(playerGuid);
                    if (!_player)
                        continue;

                    switch (_player->getClass())
                    {
                        case CLASS_WARRIOR:
                            cache3v3Queue[WARRIOR]++;
                            break;
                        case CLASS_PALADIN:
                            cache3v3Queue[PALADIN]++;
                            break;
                        case CLASS_DEATH_KNIGHT:
                            cache3v3Queue[DK]++;
                            break;
                        case CLASS_HUNTER:
                            cache3v3Queue[HUNTER]++;
                            break;
                        case CLASS_SHAMAN:
                            cache3v3Queue[SHAMAN]++;
                            break;
                        case CLASS_ROGUE:
                            cache3v3Queue[ROGUE]++;
                            break;
                        case CLASS_DRUID:
                            cache3v3Queue[DRUID]++;
                            break;
                        case CLASS_MAGE:
                            cache3v3Queue[MAGE]++;
                            break;
                        case CLASS_WARLOCK:
                            cache3v3Queue[WARLOCK]++;
                            break;
                        case CLASS_PRIEST:
                            cache3v3Queue[PRIEST]++;
                            break;
                        default:
                            break;
                    }

                    Solo3v3TalentCat plrCat = sSolo->GetTalentCatForSolo3v3(_player); // get talent cat
                    cache3v3Queue[plrCat]++;
                }
            }
        }
    }
}

namespace
{
    uint32 CalculateSelectedPoolMMR(BattlegroundQueue* queue, uint32 teamIndex)
    {
        if (!queue || teamIndex >= BG_TEAMS_COUNT)
            return 1500;

        uint64 total = 0;
        uint32 count = 0;
        for (GroupQueueInfo* group : queue->m_SelectionPools[teamIndex].SelectedGroups)
        {
            if (!group)
                continue;

            uint32 const mmr = group->ArenaMatchmakerRating ? group->ArenaMatchmakerRating : 1500;
            total += mmr;
            ++count;
        }

        return count ? uint32(total / count) : 1500;
    }
}

void Solo3v3BG::OnQueueUpdate(BattlegroundQueue* queue, uint32 /*diff*/, BattlegroundTypeId bgTypeId, BattlegroundBracketId bracket_id, uint8 arenaType, bool isRated, uint32 /*arenaRatedTeamId*/)
{
    if (arenaType != (ArenaType)ARENA_TYPE_3v3_SOLO)
        return;

    Battleground* bg_template = sBattlegroundMgr->GetBattlegroundTemplate(bgTypeId);

    if (!bg_template)
        return;

    PvPDifficultyEntry const* bracketEntry = GetBattlegroundBracketById(bg_template->GetMapId(), bracket_id);
    if (!bracketEntry)
        return;

    if (sSolo->CheckSolo3v3Arena(queue, bracket_id, isRated))
    {
        Battleground* arena = sBattlegroundMgr->CreateNewBattleground(bgTypeId, bracketEntry, arenaType, isRated);
        if (!arena)
            return;

        // Create temp arena team and store arenaTeamId
        ArenaTeam* arenaTeams[BG_TEAMS_COUNT];
        sSolo->CreateTempArenaTeamForQueue(queue, arenaTeams);

        // invite those selection pools
        for (uint32 i = 0; i < BG_TEAMS_COUNT; i++)
            for (auto const& citr : queue->m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups)
            {
                citr->ArenaTeamId = arenaTeams[i]->GetId();
                queue->InviteGroupToBG(citr, arena, citr->teamId);
            }

        // Override ArenaTeamId to temp arena team (was first set in InviteGroupToBG)
        arena->SetArenaTeamIdForTeam(TEAM_ALLIANCE, arenaTeams[TEAM_ALLIANCE]->GetId());
        arena->SetArenaTeamIdForTeam(TEAM_HORDE, arenaTeams[TEAM_HORDE]->GetId());

        SoloMatchContext context;
        context.rated = isRated;
        context.teamMMR[TEAM_ALLIANCE] = CalculateSelectedPoolMMR(queue, TEAM_ALLIANCE);
        context.teamMMR[TEAM_HORDE] = CalculateSelectedPoolMMR(queue, TEAM_HORDE);
        g_soloMatchContexts[arena->GetInstanceID()] = context;

        // The core may still inspect matchmaker ratings while resolving the temporary
        // rated battleground. Feed it the same standalone-ladder averages used below.
        arena->SetArenaMatchmakerRating(TEAM_ALLIANCE, context.teamMMR[TEAM_ALLIANCE]);
        arena->SetArenaMatchmakerRating(TEAM_HORDE, context.teamMMR[TEAM_HORDE]);

        // start bg
        arena->StartBattleground();
    }
}

bool Solo3v3BG::OnQueueUpdateValidity(BattlegroundQueue* /* queue */, uint32 /*diff*/, BattlegroundTypeId /* bgTypeId */, BattlegroundBracketId /* bracket_id */, uint8 arenaType, bool /* isRated */, uint32 /*arenaRatedTeamId*/)
{
    // if it's an arena 3v3soloQueue, return false to exit from BattlegroundQueueUpdate
    if (arenaType == (ArenaType)ARENA_TYPE_3v3_SOLO)
        return false;

    return true;
}

void Solo3v3BG::OnBattlegroundDestroy(Battleground* bg)
{
    if (bg)
        g_soloMatchContexts.erase(bg->GetInstanceID());

    sSolo->CleanUp3v3SoloQ(bg);
}

void Solo3v3BG::OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId winnerTeamId)
{
    if (!bg || !player || !bg->isRated() || bg->GetArenaType() != ARENA_TYPE_3v3_SOLO)
        return;

    auto contextItr = g_soloMatchContexts.find(bg->GetInstanceID());
    if (contextItr == g_soloMatchContexts.end() || !contextItr->second.rated)
        return;

    uint32 const guidLow = player->GetGUID().GetCounter();
    if (contextItr->second.penalizedPlayers.count(guidLow) != 0 ||
        !contextItr->second.rewardedPlayers.insert(guidLow).second)
        return;

    TeamId const playerTeam = player->GetBgTeamId();
    uint32 const ownIndex = playerTeam == TEAM_HORDE ? TEAM_HORDE : TEAM_ALLIANCE;
    uint32 const opponentIndex = ownIndex == TEAM_HORDE ? TEAM_ALLIANCE : TEAM_HORDE;
    bool const isDraw = winnerTeamId == TEAM_NEUTRAL;
    bool const isWin = !isDraw && playerTeam == winnerTeamId;

    sSolo->UpdateSoloLadderAfterMatch(
        player,
        isWin,
        isDraw,
        contextItr->second.teamMMR[ownIndex],
        contextItr->second.teamMMR[opponentIndex]);
}

void ConfigLoader3v3Arena::OnAfterConfigLoad(bool /*Reload*/)
{
    ArenaTeam::ArenaSlotByType.emplace(ARENA_TEAM_SOLO_3v3, ARENA_SLOT_SOLO_3v3);
    ArenaTeam::ArenaReqPlayersForType.emplace(ARENA_TYPE_3v3_SOLO, 6);

    BattlegroundMgr::queueToBg.insert({ BATTLEGROUND_QUEUE_3v3_SOLO, BATTLEGROUND_AA });
    BattlegroundMgr::QueueToArenaType.emplace(BATTLEGROUND_QUEUE_3v3_SOLO, (ArenaType)ARENA_TYPE_3v3_SOLO);
}

void Team3v3arena::OnGetSlotByType(const uint32 type, uint8& slot)
{
    if (type == ARENA_TYPE_3v3_SOLO)
    {
        slot = ARENA_SLOT_SOLO_3v3;
    }
}

void Team3v3arena::OnGetArenaPoints(ArenaTeam* at, float& points)
{
    if (at->GetType() == ARENA_TEAM_SOLO_3v3)
    {
        const auto Members = at->GetMembers();
        if (Members.empty())
        {
            points = 0;
            return;
        }

        uint8 playerLevel = sCharacterCache->GetCharacterLevelByGuid(Members.front().Guid);

        if (playerLevel >= sConfigMgr->GetOption<uint32>("Solo.3v3.ArenaPointsMinLevel", 19))
            points *= sConfigMgr->GetOption<float>("Solo.3v3.ArenaPointsMulti", 0.8f);
        else
            points *= 0;
    }
}

void Team3v3arena::OnTypeIDToQueueID(const BattlegroundTypeId /*bgTypeId*/, const uint8 arenaType, uint32& _bgQueueTypeId)
{
    // Keep solo queue isolated in its own queue id bucket,
    // regardless of whether caller uses our custom arena type.
    if (arenaType == ARENA_TYPE_3v3_SOLO)
        _bgQueueTypeId = bgQueueTypeId;
}

void Team3v3arena::OnQueueIdToArenaType(const BattlegroundQueueTypeId _bgQueueTypeId, uint8& arenaType)
{
    if (_bgQueueTypeId == bgQueueTypeId)
    {
        // Force client/announce/UI to treat it as 3v3 so it prints 3v3 / 3x3.
        arenaType = ARENA_TYPE_3v3; // <-- this is the key change
        return;
    }
}

void Arena_SC::OnArenaStart(Battleground* bg)
{
    if (bg->GetArenaType() != ARENA_TYPE_3v3_SOLO)
        return;

    sSolo->CheckStartSolo3v3Arena(bg);
}

void PlayerScript3v3Arena::OnPlayerBattlegroundDesertion(Player* player, const BattlegroundDesertionType type)
{
    if (!player)
        return;

    Battleground* bg = player->GetBattleground();

    switch (type)
    {
        case ARENA_DESERTION_TYPE_LEAVE_BG:

            if (bg && bg->GetArenaType() == ARENA_TYPE_3v3_SOLO)
            {
                auto contextItr = g_soloMatchContexts.find(bg->GetInstanceID());
                if (contextItr != g_soloMatchContexts.end())
                    contextItr->second.penalizedPlayers.insert(player->GetGUID().GetCounter());

                if (bg->GetStatus() == STATUS_WAIT_JOIN)
                {
                    if (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true))
                        player->CastSpell(player, 26013, true);

                    // end arena if a player leaves while in preparation
                    if (sConfigMgr->GetOption<bool>("Solo.3v3.StopGameIncomplete", true))
                    {
                        sSolo->SaveIncompleteMatchLogs(bg);
                        bg->SetRated(false);
                        bg->EndBattleground(TEAM_NEUTRAL);
                    }

                    sSolo->CountAsLoss(player, false);
                }

                if (bg->GetStatus() == STATUS_IN_PROGRESS)
                    sSolo->CountAsLoss(player, true);
            }
            break;

        case ARENA_DESERTION_TYPE_NO_ENTER_BUTTON: // called if player doesn't click 'enter arena' for solo 3v3

            if (player->IsInvitedForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
            {
                if (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true))
                    player->CastSpell(player, 26013, true);

                sSolo->CountAsLoss(player, false);
            }
            break;

        case ARENA_DESERTION_TYPE_INVITE_LOGOUT: // called if player logout when solo 3v3 queue pops (it removes the queue)

            if (player->IsInvitedForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
            {
                if (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true))
                    player->CastSpell(player, 26013, true);

                sSolo->CountAsLoss(player, false);
            }
            break;

            /*
            case ARENA_DESERTION_TYPE_LEAVE_QUEUE: // called if player uses macro to leave queue when it pops. /run AcceptBattlefieldPort(1, 0);

                // I believe these are being called AFTER the player removes the queue, so we can't know his queue
                if (player->IsInvitedForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
                {
                    LOG_ERROR("solo3v3", "IsInvitedForBattlegroundQueueType BATTLEGROUND_QUEUE_3v3_SOLO");
                    sSolo->CountAsLoss(player, false);

                }
                else if (player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
                {
                    LOG_ERROR("solo3v3", "InBattlegroundQueueForBattlegroundQueueType BATTLEGROUND_QUEUE_3v3_SOLO");
                }
                else
                {
                    LOG_ERROR("solo3v3", "ARENA_DESERTION_TYPE_LEAVE_QUEUE - else");
                }
            */

        default:
            break;
    }
}

void PlayerScript3v3Arena::OnPlayerLogin(Player* pPlayer)
{
    if (sConfigMgr->GetOption<bool>("Solo.3v3.ShowMessageOnLogin", false)) {
        ChatHandler(pPlayer->GetSession()).SendSysMessage("This server is running the |cff4CFF00Arena solo Q 3v3 |rmodule.");
    }
}

void PlayerScript3v3Arena::OnPlayerGetArenaPersonalRating(Player* player, uint8 slot, uint32& rating)
{
    if (!player || slot != ARENA_SLOT_SOLO_3v3)
        return;

    uint32 mmr = 0;
    sSolo->GetSoloRatingAndMMR(player, rating, mmr);
}

void PlayerScript3v3Arena::OnPlayerGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const
{
    if (!sConfigMgr->GetOption<bool>("Solo.3v3.VendorRating", true))
    {
        return;
    }

    if (player && minslot < 6)
    {
        uint32 soloRating = 0;
        uint32 soloMmr = 0;
        sSolo->GetSoloRatingAndMMR(const_cast<Player*>(player), soloRating, soloMmr);
        maxArenaRating = std::max(soloRating, maxArenaRating);
    }
}

void PlayerScript3v3Arena::OnPlayerGetArenaTeamId(Player* player, uint8 slot, uint32& result)
{
    if (!player)
        return;

    if (slot == ARENA_SLOT_SOLO_3v3)
        result = 0; // Standalone Solo 3v3 ladder; no persistent ArenaTeam ID.
}

bool PlayerScript3v3Arena::OnPlayerNotSetArenaTeamInfoField(Player* player, uint8 slot, ArenaTeamInfoType /* type */, uint32 /* value */)
{
    if (!player)
        return false;

    if (slot == ARENA_SLOT_SOLO_3v3)
    {
        return false;
    }

    return true;
}

bool PlayerScript3v3Arena::OnPlayerCanBattleFieldPort(Player* player, uint8 arenaType, BattlegroundTypeId BGTypeID, uint8 /*action*/)
{
    if (!player)
        return false;

    BattlegroundQueueTypeId bgQueueTypeId = BattlegroundMgr::BGQueueTypeId(BGTypeID, arenaType);
    if (bgQueueTypeId == BATTLEGROUND_QUEUE_NONE)
        return false;

    // if ((bgQueueTypeId == (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_1v1 || bgQueueTypeId == (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO
    //     && (action == 1 /*accept join*/  && !sSolo->Arena1v1CheckTalents(player)))
    //     return false;

    return true;
}



class PlayerGossip_Solo3v3Service final : public PlayerGossip
{
public:
    enum Senders
    {
        ROOT = 100
    };

    PlayerGossip_Solo3v3Service() : PlayerGossip(91011)
    {
        RegisterAction(ROOT, OpenRoot);
        RegisterAction(GOSSIP_SENDER_MAIN, Dispatch);
    }

    static void OpenRoot(Player* player, int32, int32, std::any)
    {
        NpcSolo3v3 script;
        script.OnGossipHello(player, nullptr);
    }

    static void Dispatch(Player* player, int32 sender, int32 action, std::any)
    {
        NpcSolo3v3 script;
        script.OnGossipSelect(player, nullptr, uint32(sender), uint32(action));
    }
};

namespace RTG::Services::Solo3v3
{
    bool Open(Player* player)
    {
        if (!player)
            return false;

        player->PlayerTalkClass->ClearMenus();
        CloseGossipMenuFor(player);
        sPlayerGossipMgr->ShowGossipMenu(player, 91011, PlayerGossip_Solo3v3Service::ROOT, 0);
        return true;
    }
}

void AddSC_Solo_3v3_Arena()
{
    if (!ArenaTeam::ArenaSlotByType.count(ARENA_TEAM_SOLO_3v3))
        ArenaTeam::ArenaSlotByType[ARENA_TEAM_SOLO_3v3] = ARENA_SLOT_SOLO_3v3;

    if (!ArenaTeam::ArenaReqPlayersForType.count(ARENA_TYPE_3v3_SOLO))
        ArenaTeam::ArenaReqPlayersForType[ARENA_TYPE_3v3_SOLO] = 6;

    if (!BattlegroundMgr::queueToBg.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::queueToBg[BATTLEGROUND_QUEUE_3v3_SOLO] = BATTLEGROUND_AA;

    if (!BattlegroundMgr::ArenaTypeToQueue.count(ARENA_TYPE_3v3_SOLO))
        BattlegroundMgr::ArenaTypeToQueue[ARENA_TYPE_3v3_SOLO] = (BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO;

    if (!BattlegroundMgr::QueueToArenaType.count(BATTLEGROUND_QUEUE_3v3_SOLO))
        BattlegroundMgr::QueueToArenaType[BATTLEGROUND_QUEUE_3v3_SOLO] = (ArenaType)ARENA_TYPE_3v3_SOLO;

    new NpcSolo3v3();
    new Solo3v3BG();
    new Team3v3arena();
    new ConfigLoader3v3Arena();
    new PlayerScript3v3Arena();
    new Arena_SC();
    new Solo3v3Spell();
    new PlayerGossip_Solo3v3Service();
}