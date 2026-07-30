#include "Chat.h"
#include "ObjectMgr.h"
#include "Player.h"
#include "Tokenize.h"
#include "DatabaseEnv.h"
#include "Config.h"
#include "BattlegroundMgr.h"
#include "CommandScript.h"
#include "solo3v3_sc.h"

using namespace Acore::ChatCommands;

class CommandJoinSolo : public CommandScript
{
public:
    CommandJoinSolo() : CommandScript("CommandJoinSolo") { }

    ChatCommandTable GetCommands() const override
    {
        static ChatCommandTable command3v3Table =
        {
            { "rated",       HandleQueueArena3v3Rated,         SEC_PLAYER,        Console::No },
            { "unrated",     HandleQueueArena3v3UnRated,       SEC_PLAYER,        Console::No },
            { "stats",       HandleQueueArenaSolo3v3Stats,     SEC_PLAYER,        Console::No },
        };

        static ChatCommandTable SoloCommandTable =
        {
            { "qsolo",     command3v3Table },
            { "testqsolo", HandleQueueSoloArenaTesting,  SEC_ADMINISTRATOR, Console::No }
        };

        return SoloCommandTable;
    }

    static bool HandleQueueArena3v3Rated(ChatHandler* handler, const char* args)
    {
        return HandleQueueSoloArena(handler, args, true);
    }

    static bool HandleQueueArena3v3UnRated(ChatHandler* handler, const char* args)
    {
        return HandleQueueSoloArena(handler, args, false);
    }

    static bool HandleQueueSoloArena(ChatHandler* handler, const char* /*args*/, bool isRated)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sConfigMgr->GetOption<bool>("Solo.3v3.EnableCommand", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena command is disabled.");
            return false;
        }

        if (!sConfigMgr->GetOption<bool>("Solo.3v3.Enable", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena is disabled.");
            return false;
        }

        if (player->IsInCombat())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Can't be in combat.");
            return false;
        }

        NpcSolo3v3 SoloCommand;
        if (player->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
        {
            WorldPacket data;
            sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
            player->GetSession()->SendPacket(&data);
            return false;
        }

        uint32 minLevel = sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 19);
        if (player->GetLevel() < minLevel)
        {
            ChatHandler(player->GetSession()).PSendSysMessage("You need level {}+ to join solo arena.", minLevel);
            return false;
        }

        if (isRated && !sSolo->IsRatedEnabled())
        {
            handler->SendSysMessage("Rated Solo 3v3 is currently disabled.");
            return false;
        }

        if (!SoloCommand.ArenaCheckFullEquipAndTalents(player))
            return false;

        if (SoloCommand.JoinQueueArena(player, nullptr, isRated))
            handler->PSendSysMessage("You have joined the solo 3v3 arena queue {}.", isRated ? "rated" : "unrated");

        return true;
    }

    static bool HandleQueueArenaSolo3v3Stats(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        uint32 rating = 0;
        uint32 mmr = 0;
        uint32 games = 0;
        uint32 wins = 0;
        uint32 losses = 0;
        sSolo->GetSoloStats(player, rating, mmr, games, wins, losses);

        handler->PSendSysMessage(
            "=== Solo 3v3 Statistics ===\nRating: {}\nMMR: {}\nGames: {}\nWins: {}\nLosses: {}",
            rating, mmr, games, wins, losses);
        return true;
    }

    // USED IN TESTING ONLY!!! (time saving when alt tabbing) Will join solo 3v3 on all players!
    // also use macros: /run AcceptBattlefieldPort(1,1); to accept queue and /afk to leave arena
    static bool HandleQueueSoloArenaTesting(ChatHandler* handler, const char* /*args*/)
    {
        Player* player = handler->GetSession()->GetPlayer();
        if (!player)
            return false;

        if (!sConfigMgr->GetOption<bool>("Solo.3v3.EnableTestingCommand", false))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena testing command is disabled.");
            return false;
        }

        if (!sConfigMgr->GetOption<bool>("Solo.3v3.Enable", true))
        {
            ChatHandler(player->GetSession()).SendSysMessage("Solo 3v3 Arena is disabled.");
            return false;
        }

        if (!sSolo->IsRatedEnabled())
        {
            ChatHandler(player->GetSession()).SendSysMessage("Rated Solo 3v3 is unavailable until its character schema is installed.");
            return false;
        }

        NpcSolo3v3 SoloCommand;
        for (auto& pair : ObjectAccessor::GetPlayers())
        {
            Player* currentPlayer = pair.second;
            if (currentPlayer)
            {
                if (currentPlayer->IsInCombat())
                {
                    handler->PSendSysMessage("Player {} can't be in combat.", currentPlayer->GetName().c_str());
                    continue;
                }

                if (currentPlayer->HasAura(26013) && (sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnAfk", true) || sConfigMgr->GetOption<bool>("Solo.3v3.CastDeserterOnLeave", true)))
                {
                    WorldPacket data;
                    sBattlegroundMgr->BuildGroupJoinedBattlegroundPacket(&data, ERR_GROUP_JOIN_BATTLEGROUND_DESERTERS);
                    currentPlayer->GetSession()->SendPacket(&data);
                    continue;
                }

                uint32 minLevel = sConfigMgr->GetOption<uint32>("Solo.3v3.MinLevel", 19);
                if (currentPlayer->GetLevel() < minLevel)
                {
                    handler->PSendSysMessage("Player {} needs level {}+ to join solo arena.", player->GetName().c_str(), minLevel);
                    continue;
                }

                if (!SoloCommand.ArenaCheckFullEquipAndTalents(currentPlayer))
                    continue;

                if (SoloCommand.JoinQueueArena(currentPlayer, nullptr, true))
                    handler->PSendSysMessage("Player {} has joined the solo 3v3 arena queue.", currentPlayer->GetName().c_str());
                else
                    handler->PSendSysMessage("Failed to join queue for player {}.", currentPlayer->GetName().c_str());
            }
        }

        return true;
    }
};

void AddSC_Solo_3v3_commandscript()
{
    new CommandJoinSolo();
}
