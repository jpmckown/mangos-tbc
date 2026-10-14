/* This file is part of the ScriptDev2 Project. See AUTHORS file for Copyright information
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ScriptData
SDName: Zulfarrak
SD%Complete: 100
SDComment:
SDCategory: Zul'Farrak
EndScriptData */

/* ContentData
event_go_zulfarrak_gong
event_spell_unlocking
at_zulfarrak
npc_zulfarrak_prisoner
EndContentData */

#include "AI/EventAI/CreatureEventAI.h"
#include "AI/ScriptDevAI/include/sc_common.h"
#include "AI/ScriptDevAI/include/sc_solo_scaling.h"
#include "zulfarrak.h"

/*######
## event_go_zulfarrak_gong
######*/

bool ProcessEventId_event_go_zulfarrak_gong(uint32 /*uiEventId*/, Object* pSource, Object* /*pTarget*/, bool bIsStart)
{
    if (bIsStart && pSource->GetTypeId() == TYPEID_PLAYER)
    {
        if (instance_zulfarrak* pInstance = (instance_zulfarrak*)((Player*)pSource)->GetInstanceData())
        {
            if (pInstance->GetData(TYPE_GAHZRILLA) == NOT_STARTED || pInstance->GetData(TYPE_GAHZRILLA) == FAIL)
            {
                pInstance->SetData(TYPE_GAHZRILLA, IN_PROGRESS);
                return false;                               // Summon Gahz'rilla by Database Script
            }
            return true;
            // Prevent DB script summoning Gahz'rilla
        }
    }
    return false;
}

/*######
## event_spell_unlocking
######*/

bool ProcessEventId_event_spell_unlocking(uint32 /*uiEventId*/, Object* pSource, Object* /*pTarget*/, bool bIsStart)
{
    if (bIsStart && pSource->GetTypeId() == TYPEID_PLAYER)
    {
        if (instance_zulfarrak* pInstance = (instance_zulfarrak*)((Player*)pSource)->GetInstanceData())
        {
            if (pInstance->GetData(TYPE_PYRAMID_EVENT) == NOT_STARTED)
            {
                pInstance->SetData(TYPE_PYRAMID_EVENT, IN_PROGRESS);
                return false;                               // Summon pyramid trolls by Database Script
            }
            return true;
        }
    }
    return false;
}

/*######
## at_zulfarrak
######*/

bool AreaTrigger_at_zulfarrak(Player* pPlayer, AreaTriggerEntry const* pAt)
{
    if (pAt->id == AREATRIGGER_ANTUSUL)
    {
        if (pPlayer->IsGameMaster() || pPlayer->IsDead())
            return false;

        instance_zulfarrak* pInstance = (instance_zulfarrak*)pPlayer->GetInstanceData();

        if (!pInstance)
            return false;

        if (pInstance->GetData(TYPE_ANTUSUL) == NOT_STARTED || pInstance->GetData(TYPE_ANTUSUL) == FAIL)
        {
            if (Creature* pAntuSul = pInstance->GetSingleCreatureFromStorage(NPC_ANTUSUL))
            {
                if (pAntuSul->IsAlive())
                    pAntuSul->AI()->AttackStart(pPlayer);
            }
        }
    }

    return false;
}

/*######
## npc_zulfarrak_prisoner
######*/

// Fork (solo): Sergeant Bly, Raven, Oro Eyegouge, Weegli Blastfuse and Murta Grimgut keep their EventAI; while they fight
// the pyramid trolls the damage creatures deal them scales with the player count (24% solo, stock with 5 players).
// Damage from players (the optional fight once the crew turns hostile) is not scaled.
struct npc_zulfarrak_prisonerAI : public CreatureEventAI
{
    npc_zulfarrak_prisonerAI(Creature* creature) : CreatureEventAI(creature) {}

    void DamageTaken(Unit* dealer, uint32& damage, DamageEffectType damageType, SpellEntry const* spellInfo) override
    {
        if (damageType != INSTAKILL && dealer && !dealer->IsPlayerControlled())
            damage = uint32(damage * EscortDamageTakenFactor(m_creature->GetMap(), 5));

        CreatureEventAI::DamageTaken(dealer, damage, damageType, spellInfo);
    }
};

void AddSC_zulfarrak()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "event_go_zulfarrak_gong";
    pNewScript->pProcessEventId = &ProcessEventId_event_go_zulfarrak_gong;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "event_spell_unlocking";
    pNewScript->pProcessEventId = &ProcessEventId_event_spell_unlocking;
    pNewScript->RegisterSelf();

    pNewScript = new Script;
    pNewScript->Name = "at_zulfarrak";
    pNewScript->pAreaTrigger = &AreaTrigger_at_zulfarrak;
    pNewScript->RegisterSelf();

    pNewScript = new Script;                                // Fork (solo)
    pNewScript->Name = "npc_zulfarrak_prisoner";
    pNewScript->GetAI = &GetNewAIInstance<npc_zulfarrak_prisonerAI>;
    pNewScript->RegisterSelf();
}
