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
#include <unordered_map>

struct SoloMatchContext
{
    bool rated = false;
    uint32 teamMMR[BG_TEAMS_COUNT] = { 1500, 1500 };
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

    if (player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_2v2_7:30:30:-18:0|t Leave Solo queue", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_LEAVE_QUEUE, "Are you sure you want to remove the solo queue?", 0, false);

    if (!player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_3v3_5:30:30:-18:0|t Queue 3v3soloQ (UnRated)\n", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_UNRATED);

    if (!player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3))
    {
        uint32 cost = sConfigMgr->GetOption<uint32>("Solo.3v3.Cost", 1);
        if (player->IsPvP())
            cost = 0;

        AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_2v2_7:30:30:-18:0|t  Create new Solo arena team", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_CREATE_ARENA_TEAM, "Create new solo arena team?", cost, false);
    }
    else
    {
        if (!player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
        {
            AddGossipItemFor(player, GOSSIP_ICON_INTERACT_1, "|TInterface/ICONS/Achievement_Arena_3v3_5:30:30:-18:0|t Queue 3v3soloQ (Rated)\n", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_JOIN_QUEUE_ARENA_RATED);
            AddGossipItemFor(player, GOSSIP_ICON_DOT, "|TInterface/ICONS/Achievement_Arena_2v2_7:30:30:-18:0|t Disband Arena team", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_DISBAND_ARENATEAM, "Are you sure?", 0, false);
        }

        AddGossipItemFor(player, GOSSIP_ICON_DOT, "|TInterface/ICONS/INV_Misc_Coin_01:30:30:-18:0|t Show statistics", GOSSIP_SENDER_MAIN, NPC_3v3_ACTION_GET_STATISTICS);
    }

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
            if (sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80) <= player->GetLevel())
            {
                int cost = sConfigMgr->GetOption<uint32>("Solo.3v3.Cost", 1);

                if (player->IsPvP())
                    cost = 0;

                if (cost >= 0 && player->GetMoney() >= uint32(cost) && CreateArenateam(player, creature))
                    player->ModifyMoney(sConfigMgr->GetOption<uint32>("Solo.3v3.Cost", 1) * -1);
            }
            else
            {
                ChatHandler(player->GetSession()).PSendSysMessage("You need level {}+ to create an arena team.", sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80));
            }

            CloseGossipMenuFor(player);
            return true;
        }
        break;

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
            if (player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_3v3_SOLO))
            {
                uint8 arenaType = ARENA_TYPE_3v3_SOLO;

                WorldPacket Data;
                Data << arenaType << (uint8)0x0 << (uint32)BATTLEGROUND_AA << (uint16)0x0 << (uint8)0x0;
                player->GetSession()->HandleBattleFieldPortOpcode(Data);
                CloseGossipMenuFor(player);

            }
            return true;
        }

        case NPC_3v3_ACTION_GET_STATISTICS:
        {
            ArenaTeam* at = sArenaTeamMgr->GetArenaTeamById(player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3));
            if (at)
            {
                std::stringstream s;
                s << "Rating: " << at->GetStats().Rating;
                s << "\nPersonal Rating: " << player->GetArenaPersonalRating(ARENA_SLOT_SOLO_3v3);
                s << "\nRank: " << at->GetStats().Rank;
                s << "\nSeason Games: " << at->GetStats().SeasonGames;
                s << "\nSeason Wins: " << at->GetStats().SeasonWins;
                s << "\nWeek Games: " << at->GetStats().WeekGames;
                s << "\nWeek Wins: " << at->GetStats().WeekWins;

                ChatHandler(player->GetSession()).PSendSysMessage("{}", s.str().c_str());
                CloseGossipMenuFor(player);

                ArenaTeam::MemberList::iterator itr;
                for (itr = at->GetMembers().begin(); itr != at->GetMembers().end(); ++itr)
                {
                    if (itr->Guid == player->GetGUID())
                    {
                        std::stringstream s;
                        s << "\nSolo MMR: " << itr->MatchMakerRating;

                        ChatHandler(player->GetSession()).PSendSysMessage("{}", s.str().c_str());
                        break;
                    }
                }
            }

            return true;
        }
        case NPC_3v3_ACTION_DISBAND_ARENATEAM:
        {
            WorldPacket Data;
            Data << player->GetArenaTeamId(ARENA_SLOT_SOLO_3v3);
            player->GetSession()->HandleArenaTeamLeaveOpcode(Data);
            ChatHandler(player->GetSession()).PSendSysMessage("Arena team deleted!");
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

    if (sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 80) > player->GetLevel())
        return false;

    uint8 arenatype = ARENA_TYPE_3v3_SOLO;
    uint32 arenaRating = 0;
    uint32 matchmakerRating = 0;

    // ignore if we already in BG, Arena or Arena queue
    if (player->InBattleground() || player->InArena() || player->InBattlegroundQueueForBattlegroundQueueType((BattlegroundQueueTypeId)BATTLEGROUND_QUEUE_2v2) ||
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
    if (player->GetBattlegroundQueueIndex(bgQueueTypeId) < PLAYER_MAX_BATTLEGROUND_QUEUES)
        return false; // player is already in this queue

    // check if has free queue slots
    if (!player->HasFreeBattlegroundQueueId())
        return false;

    uint32 ateamId = 0;

    if (isRated)
    {
        // Rated solo uses the separate solo ladder (does not require / modify ArenaTeams)
        uint32 soloRating = 1500;
        uint32 soloMMR = 1500;
        sSolo->GetSoloRatingAndMMR(player, soloRating, soloMMR);

        arenaRating = soloRating;
        matchmakerRating = soloMMR;
        ateamId = 0;
    }

    BattlegroundQueue& bgQueue = sBattlegroundMgr->GetBattlegroundQueue(bgQueueTypeId);
    BattlegroundTypeId bgTypeId = BATTLEGROUND_AA;

    bg->SetRated(false);
    bg->SetMinPlayersPerTeam(3);

    GroupQueueInfo* ginfo = bgQueue.AddGroup(player, nullptr, bgTypeId, bracketEntry, arenatype, isRated, false, arenaRating, matchmakerRating, ateamId, 0);
    uint32 avgTime = bgQueue.GetAverageQueueWaitTime(ginfo);
    uint32 queueSlot = player->AddBattlegroundQueueId(bgQueueTypeId);

    // send status packet (in queue)
    WorldPacket data;
    sBattlegroundMgr->BuildBattlegroundStatusPacket(&data, bg, queueSlot, STATUS_WAIT_QUEUE, avgTime, 0, arenatype, TEAM_NEUTRAL, isRated);
    player->GetSession()->SendPacket(&data);

    if (isRated && matchmakerRating == 0) {
        matchmakerRating = 1;
    }

    sBattlegroundMgr->ScheduleQueueUpdate(matchmakerRating, ARENA_TYPE_3v3_SOLO, bgQueueTypeId, bgTypeId, bracketEntry->GetBracketId());

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

    if (!arenaTeam->Create(player->GetGUID(), uint8(ARENA_TYPE_3v3_SOLO), teamName.str(), 4283124816, 45, 4294242303, 5, 4294705149))
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
        // Solo queue creates a normal 3v3 arena match.
        // Even when players queue "rated", we track solo rating separately and keep the core match unrated
        // to avoid any ArenaTeam dependency / crashes.
        Battleground* arena = sBattlegroundMgr->CreateNewBattleground(bgTypeId, bracketEntry, arenaType, false);
        if (!arena)
            return;

        // Build match context (rated/unrated + team average MMR from queue data)
        SoloMatchContext ctx;
        ctx.rated = isRated;

        for (uint32 i = 0; i < BG_TEAMS_COUNT; ++i)
        {
            uint32 sum = 0;
            uint32 count = 0;
            for (auto const& g : queue->m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups)
            {
                sum += g->ArenaMatchmakerRating;
                count += 1;
            }
            ctx.teamMMR[i] = count ? (sum / count) : 1500;
        }

        g_soloMatchContexts[arena->GetInstanceID()] = ctx;

        // Invite selected groups
        for (uint32 i = 0; i < BG_TEAMS_COUNT; i++)
            for (auto const& citr : queue->m_SelectionPools[TEAM_ALLIANCE + i].SelectedGroups)
                queue->InviteGroupToBG(citr, arena, citr->teamId);

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
    if (!bg || !player)
        return;

    if (bg->GetArenaType() != ARENA_TYPE_3v3_SOLO)
        return;

    auto it = g_soloMatchContexts.find(bg->GetInstanceID());
    if (it == g_soloMatchContexts.end() || !it->second.rated)
        return; // unrated solo: no ladder changes

    // Draw: count game, no rating change
    bool isDraw = (winnerTeamId == TEAM_NEUTRAL);

    TeamId myTeam = player->GetBgTeamId();
    uint32 myIdx = (myTeam == TEAM_HORDE) ? TEAM_HORDE : TEAM_ALLIANCE;
    uint32 oppIdx = (myIdx == TEAM_HORDE) ? TEAM_ALLIANCE : TEAM_HORDE;

    bool isWin = (!isDraw) && (myTeam == winnerTeamId);

    uint32 myTeamMMR = it->second.teamMMR[myIdx];
    uint32 oppTeamMMR = it->second.teamMMR[oppIdx];

    sSolo->UpdateSoloLadderAfterMatch(player, isWin, isDraw, myTeamMMR, oppTeamMMR);
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
    if (!player)
        return;

    if (slot == ARENA_SLOT_SOLO_3v3)
    {
        uint32 soloRating = 1500;
        uint32 soloMMR = 1500;
        sSolo->GetSoloRatingAndMMR(player, soloRating, soloMMR);
        rating = soloRating;
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
        uint32 soloRating = 1500;
        uint32 soloMMR = 1500;
        sSolo->GetSoloRatingAndMMR(const_cast<Player*>(player), soloRating, soloMMR);
        maxArenaRating = std::max(soloRating, maxArenaRating);
    }
}

void PlayerScript3v3Arena::OnPlayerGetArenaTeamId(Player* player, uint8 slot, uint32& result)
{
    if (!player)
        return;

    if (slot == ARENA_SLOT_SOLO_3v3)
        result = 0; // Solo ladder does not use ArenaTeam IDs
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
