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
#include "AccountMgr.h"
#include <unordered_map>

struct ArenaTeamsRating {
    uint32 allianceRating;
    uint32 hordeRating;
    uint8 playersCount = 0;
};
std::unordered_map<uint32, ArenaTeamsRating> bgArenaTeamsRating;

void NpcSolo3v3::Initialize()
{
    for (int i = 0; i < MAX_TALENT_CAT; i++)
        cache3v3Queue[i] = 0;

    lastFetchQueueList = 0;
}

bool NpcSolo3v3::OnGossipHello(Player* player, Creature* creature)
{
    if (!player || !creature)
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
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_3v3_5:30:30:-18:0|t Queue Solo 3v3 (Skirmish)\n", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_UNRATED);
", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_UNRATED);

    // Rated queue does NOT require an ArenaTeam anymore (separate solo ladder).
    // Show the Rated option whenever the player is not already in the solo queue.
    bool ratedEnabled = sConfigMgr->GetOption<bool>("Solo.3v3.Rated.Enable", true);
    if (ratedEnabled && !inSoloQueue && !inNormal3v3)
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_3v3_5:30:30:-18:0|t Queue 3v3soloQ (Rated)\n", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED);


    // Solo Queue uses a separate ladder table and does NOT require a permanent ArenaTeam.
    // Keep the NPC UI focused on queueing + stats (no create/disband team).
    AddGossipItemFor(player, GOSSIP_ICON_DOT, "|TInterface/ICONS/INV_Misc_Coin_01:30:30:-18:0|t Show statistics", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_GET_STATISTICS);

    AddGossipItemFor(player, GOSSIP_ICON_CHAT, "|TInterface/ICONS/inv_misc_questionmark:30:30:-20:0|t Help", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_SCRIPT_INFO);

    SendGossipMenuFor(player, 60015, creature->GetGUID());

    return true;
}

bool NpcSolo3v3::OnGossipSelect(Player* player, Creature* creature, uint32 /*sender*/, uint32 action)
{
    if (!player || !creature)
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
            SendGossipMenuFor(player, NPC_TEXT_3v3, creature->GetGUID());
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

    // Rated: block playerbots / rndbot accounts from ever queueing rated.
    if (isRated)
    {
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

    // check if already in queue
    if (player->GetBattlegroundQueueIndex(queueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return false;

    // check if has free queue slots
    if (!player->HasFreeBattlegroundQueueId())
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
    if (!player)
        return false;

    // Check if player is already in an arena team
    if (player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3))
    {
        player->GetSession()->SendArenaTeamCommandResult(ERR_ARENA_TEAM_CREATE_S, player->GetName(), "", ERR_ALREADY_IN_ARENA_TEAM);
        return false;
    }

    // Teamname = playername
    // if team name exist, we have to choose another name (playername + number)
    int i = 1;
    std::stringstream teamName;
    teamName << player->GetName();

    do
    {
        if (sArenaTeamMgr->GetArenaTeamByName(teamName.str()) != NULL) // teamname exist, so choose another name
        {
            teamName.str(std::string());
            teamName << player->GetName() << i++;
        }
        else
            break;
    }
    while (i < 100); // should never happen

    // Create arena team
    ArenaTeam* arenaTeam = new ArenaTeam();

    if (!arenaTeam->Create(player->GetGUID(), uint8(ARENA_TYPE_3v3), teamName.str(), 4283124816, 45, 4294242303, 5, 4294705149))
    {
        delete arenaTeam;
        return false;
    }

    // Register arena team
    sArenaTeamMgr->AddArenaTeam(arenaTeam);

    ChatHandler(player->GetSession()).SendSysMessage("Arena team successful created!");

    return true;
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

        if (isRated) {
            ArenaTeamsRating arenaTeamsRating;

            arenaTeamsRating.allianceRating = arenaTeams[TEAM_ALLIANCE]->GetStats().Rating;
            arenaTeamsRating.hordeRating = arenaTeams[TEAM_HORDE]->GetStats().Rating;

            bgArenaTeamsRating[arena->GetInstanceID()] = arenaTeamsRating;
        }

        // Set matchmaker rating for calculating rating-modifier on EndBattleground (when a team has won/lost)
        arena->SetArenaMatchmakerRating(TEAM_ALLIANCE, sSolo->GetAverageMMR(arenaTeams[TEAM_ALLIANCE]));
        arena->SetArenaMatchmakerRating(TEAM_HORDE, sSolo->GetAverageMMR(arenaTeams[TEAM_HORDE]));

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
    sSolo->CleanUp3v3SoloQ(bg);
}

void Solo3v3BG::OnBattlegroundEndReward(Battleground* bg, Player* player, TeamId winnerTeamId)
{
    if (bg->isRated() && bg->GetArenaType() == ARENA_TYPE_3v3_SOLO)
    {
        // this way we always get the correct solo team (sometimes when using GetArenaTeamByCaptain inside solo arena it can return a teamID >= 4293918720)
        ArenaTeam* plrArenaTeam = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3));

        if (!plrArenaTeam)
            return;

        ArenaTeamStats atStats = plrArenaTeam->GetStats();

        bgArenaTeamsRating[bg->GetInstanceID()].playersCount += 1;

        atStats.SeasonGames += 1;
        atStats.WeekGames += 1;

        // Draw: do not modify rating or MMR
        if (winnerTeamId == TEAM_NEUTRAL)
        {
            for (ArenaTeam::MemberList::iterator itr = plrArenaTeam->GetMembers().begin(); itr != plrArenaTeam->GetMembers().end(); ++itr)
            {
                if (itr->Guid == player->GetGUID())
                {
                    itr->WeekGames += 1;
                    itr->SeasonGames += 1;
                    break;
                }
            }

            plrArenaTeam->SetArenaTeamStats(atStats);
            plrArenaTeam->NotifyStatsChanged();
            plrArenaTeam->SaveToDB(true);

            if (bgArenaTeamsRating[bg->GetInstanceID()].playersCount == bg->GetPlayersSize())
                bgArenaTeamsRating.erase(bg->GetInstanceID());

            return;
        }

        int32 ratingModifier;
        int32 oldTeamRating;

        uint32 oldTeamRatingAlliance = bgArenaTeamsRating[bg->GetInstanceID()].allianceRating;
        uint32 oldTeamRatingHorde = bgArenaTeamsRating[bg->GetInstanceID()].hordeRating;

        TeamId bgTeamId = player->GetBgTeamId();
        const bool isPlayerWinning = bgTeamId == winnerTeamId;
        if (isPlayerWinning) {
            ArenaTeam* winnerArenaTeam = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(winnerTeamId));
            oldTeamRating = winnerTeamId == TEAM_HORDE ? oldTeamRatingHorde : oldTeamRatingAlliance;
            ratingModifier = int32(winnerArenaTeam->GetRating()) - oldTeamRating;

            atStats.SeasonWins += 1;
            atStats.WeekWins += 1;
        } else {
            ArenaTeam* loserArenaTeam  = sArenaTeamMgr->GetArenaTeamById(bg->GetArenaTeamIdForTeam(bg->GetOtherTeamId(winnerTeamId)));
            oldTeamRating = winnerTeamId == TEAM_HORDE ? oldTeamRatingAlliance : oldTeamRatingHorde;
            ratingModifier = int32(loserArenaTeam->GetRating()) - oldTeamRating;
        }

        if (int32(atStats.Rating) + ratingModifier < 0)
            atStats.Rating = 0;
        else
            atStats.Rating += ratingModifier;

        // Update team's rank, start with rank 1 and increase until no team with more rating was found
        atStats.Rank = 1;
        ArenaTeamMgr::ArenaTeamContainer::const_iterator i = sArenaTeamMgr->GetArenaTeamMapBegin();
        for (; i != sArenaTeamMgr->GetArenaTeamMapEnd(); ++i) {
            if (i->second->GetType() == ARENA_TEAM_SOLO_3v3 && i->second->GetStats().Rating > atStats.Rating)
                ++atStats.Rank;
        }

        for (ArenaTeam::MemberList::iterator itr = plrArenaTeam->GetMembers().begin(); itr != plrArenaTeam->GetMembers().end(); ++itr)
        {
            if (itr->Guid == player->GetGUID())
            {
                itr->PersonalRating = atStats.Rating;
                itr->WeekGames += 1;
                itr->SeasonGames += 1;

                if (isPlayerWinning) {
                    itr->WeekWins += 1;
                    itr->SeasonWins += 1;
                    itr->MatchMakerRating += ratingModifier;
                    itr->MaxMMR = std::max(itr->MaxMMR, itr->MatchMakerRating);
                } else {
                    if (int32(itr->MatchMakerRating) + ratingModifier < 0)
                        itr->MatchMakerRating = 0;
                    else
                        itr->MatchMakerRating += ratingModifier;
                }

                break;
            }

        }

        plrArenaTeam->SetArenaTeamStats(atStats);
        plrArenaTeam->NotifyStatsChanged();
        plrArenaTeam->SaveToDB(true);

        // if all the players rating have been processed, delete the stored bg rating informations
        if (bgArenaTeamsRating[bg->GetInstanceID()].playersCount == bg->GetPlayersSize())
            bgArenaTeamsRating.erase(bg->GetInstanceID());
    }
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
        uint8 playerLevel = sCharacterCache->GetCharacterLevelByGuid(Members.front().Guid);

        if (playerLevel >= sConfigMgr->GetOption<uint32>("Solo.3v3.ArenaPointsMinLevel", 70))
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
    Battleground* bg = ((BattlegroundMap*)player->FindMap())->GetBG();

    switch (type)
    {
        case ARENA_DESERTION_TYPE_LEAVE_BG:

            if (bg->GetArenaType() == ARENA_TYPE_3v3_SOLO)
            {
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
    if (slot == ARENA_SLOT_SOLO_3v3)
    {
        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TEAM_SOLO_3v3))
        {
            rating = at->GetRating();
        }
    }
}

void PlayerScript3v3Arena::OnPlayerGetMaxPersonalArenaRatingRequirement(const Player* player, uint32 minslot, uint32& maxArenaRating) const
{
    if (!sConfigMgr->GetOption<bool>("Solo.3v3.VendorRating", true))
    {
        return;
    }

    if (minslot < 6)
    {
        if (ArenaTeam* at = sArenaTeamMgr->GetArenaTeamByCaptain(player->GetGUID(), ARENA_TEAM_SOLO_3v3))
        {
            maxArenaRating = std::max(at->GetRating(), maxArenaRating);
        }
    }
}

void PlayerScript3v3Arena::OnPlayerGetArenaTeamId(Player* player, uint8 slot, uint32& result)
{
    if (!player)
        return;

    if (slot == ARENA_SLOT_SOLO_3v3)
        result = player->GetArenaTeamIdFromDB(player->GetGUID(), ARENA_TEAM_SOLO_3v3);
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
}