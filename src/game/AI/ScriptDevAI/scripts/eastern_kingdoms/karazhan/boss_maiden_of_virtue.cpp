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
SDName: Boss_Maiden_of_Virtue
SD%Complete: 100
SDComment:
SDCategory: Karazhan
EndScriptData */

#include "AI/ScriptDevAI/include/sc_common.h"
#include "karazhan.h"
#include "AI/ScriptDevAI/base/CombatAI.h"
#include "Spells/Scripts/SpellScript.h"
#include "AI/ScriptDevAI/include/sc_solo_scaling.h"

enum
{
    SAY_AGGRO               = 15079,
    SAY_SLAY1               = 15082,
    SAY_SLAY2               = 15083,
    SAY_SLAY3               = 15084,
    SAY_REPENTANCE1         = 15080,
    SAY_REPENTANCE2         = 13396,
    SAY_DEATH               = 15081,

    SPELL_REPENTANCE        = 29511,
    SPELL_HOLYFIRE          = 29522,
    SPELL_HOLYWRATH         = 32445,
    SPELL_HOLYGROUND        = 29512
};

enum MaidenOfVirtueActions
{
    MAIDEN_ACTION_MAX,
};

struct boss_maiden_of_virtueAI : public CombatAI
{
    boss_maiden_of_virtueAI(Creature* creature) : CombatAI(creature, MAIDEN_ACTION_MAX), m_instance(static_cast<ScriptedInstance*>(creature->GetInstanceData()))
    {
        m_creature->GetCombatManager().SetLeashingCheck([&](Unit*, float x, float y, float /*z*/)
        {
            return y > -2071.f && x > -10924.f;
        });

        AddOnKillText(SAY_SLAY1, SAY_SLAY2, SAY_SLAY3);
    }

    ScriptedInstance* m_instance;

    void JustDied(Unit* /*killer*/) override
    {
        DoBroadcastText(SAY_DEATH, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, DONE);
    }

    void Aggro(Unit* /*who*/) override
    {
        DoBroadcastText(SAY_AGGRO, m_creature);

        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, IN_PROGRESS);
    }

    void JustReachedHome() override
    {
        if (m_instance)
            m_instance->SetData(TYPE_MAIDEN, FAIL);
    }

    void OnSpellCast(SpellEntry const* spellInfo, Unit* /*target*/) override
    {
        switch (spellInfo->Id)
        {
            case SPELL_REPENTANCE:
                DoBroadcastText(urand(0, 1) ? SAY_REPENTANCE1 : SAY_REPENTANCE2, m_creature);
                break;
        }
    }
};

// Fork (solo): 29511 Repentance skips Maiden's victim when it is a player (Spell.cpp), so it only reaches a solo player when a pet tanks.
// Then the player's 12 s incapacitate becomes 3 s (long-CC rule), so the pet is not left without its owner for 12 s.
struct MaidenRepentance : public AuraScript
{
    int32 OnDurationCalculate(WorldObject const* /*caster*/, Unit const* target, int32 duration) const override
    {
        if (target && target->GetTypeId() == TYPEID_PLAYER && GetEncounterPlayerCount(target->GetMap()) <= 1)
            return std::min(duration, 3000);

        return duration;
    }
};

void AddSC_boss_maiden_of_virtue()
{
    Script* pNewScript = new Script;
    pNewScript->Name = "boss_maiden_of_virtue";
    pNewScript->GetAI = &GetNewAIInstance<boss_maiden_of_virtueAI>;
    pNewScript->RegisterSelf();

    RegisterSpellScript<MaidenRepentance>("spell_maiden_repentance");
}
