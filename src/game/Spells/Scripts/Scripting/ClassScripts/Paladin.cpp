/*
* This file is part of the CMaNGOS Project. See AUTHORS file for Copyright information
*
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

#include "Spells/Scripts/SpellScript.h"
#include "Spells/SpellAuras.h"
#include "Spells/SpellMgr.h"
#include "Groups/Group.h"

// 21082 - Seal of the Crusader
struct SealOfTheCrusader : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
        {
            // Seal of the Crusader damage reduction
            // SotC increases attack speed but reduces damage to maintain the same DPS
            float reduction = (-100.0f * aura->GetModifier()->m_amount) / (aura->GetModifier()->m_amount + 100.0f);
            aura->GetTarget()->HandleStatModifier(UNIT_MOD_DAMAGE_MAINHAND, TOTAL_PCT, reduction, apply);
            return;
        }

        if (aura->GetEffIndex() == EFFECT_INDEX_2)
        {
            aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_DONE, apply);
            return;
        }
    }

    void OnDamageCalculate(Aura* /*aura*/, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        totalMod *= 1.4f; // Patch 2.4.2 - Increases damage of Crusader Strike by 40%
    }
};

// 31801 - Seal of Vengeance
struct SealOfVengeance : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
        {
            aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_DONE, apply);
            return;
        }
    }

    void OnDamageCalculate(Aura* /*aura*/, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        totalMod *= 1.4f; // Patch 2.4.2 - Increases damage of Crusader Strike by 40%
    }
};


enum
{
    SPELL_RIGHTEOUS_JUDGEMENT       = 55019,
    SPELL_DIVINE_STORM_HEAL         = 55021,
    SPELL_JUDGEMENT_OF_THE_CRUSADER_HEALING = 55022,
    SPELL_VISUAL_KIT_DIVINE_STORM_HEAL = 10812, // client SpellVisualKit: the ported WotLK heal effect
    SPELL_JUDGEMENTS_OF_THE_WISE_R1 = 31876,
    SPELL_JUDGEMENTS_OF_THE_WISE_R2 = 31877,
    SPELL_JUDGEMENTS_OF_THE_WISE_R3 = 31878,
    SPELL_JUDGEMENTS_OF_THE_WISE_MANA = 31930,
    SPELL_REPLENISHMENT             = 55007,
    SPELL_WARDING_MANA              = 55023,
    SPELL_SEAL_OF_JUSTICE_STUN      = 20170,
    SPELL_SEAL_OF_JUSTICE_SLOW      = 55024,
    SPELL_JUDGEMENT_OF_JUSTICE_R1   = 20184,
    SPELL_JUDGEMENT_OF_JUSTICE_R2   = 31896,
    SPELL_JUDGEMENT_OF_JUSTICE_INTERRUPT = 55025,
    SPELL_JUDGEMENT_DEBUFF          = 55100,
    SPELL_JUDGEMENT_MANA            = 55101,
    SPELL_IMPROVED_RETRIBUTION_AURA_PULSE = 55026,
    SPELL_DIVINE_PURPOSE_R1         = 31871,
    SPELL_DIVINE_PURPOSE_R2         = 31872,
    SPELL_DIVINE_PURPOSE_R3         = 31873,
    SPELL_CATEGORY_AVENGERS_SHIELD  = 1158,
    SPELL_ANTICIPATION_FREE_SHIELD  = 55027,
    SPELL_ARDENT_DEFENDER_COOLDOWN  = 55028,
    SPELL_CONCENTRATION_AURA        = 19746,
    SPELL_IMPROVED_CONCENTRATION_PERSONAL = 55029,
    SPELL_LIGHTS_GRACE_INSTANT      = 55030,
    SPELL_AURA_MASTERY_IMMUNITY     = 55031,
    SPELL_IMPROVED_LAY_ON_HANDS_R1  = 20234,
    SPELL_IMPROVED_LAY_ON_HANDS_R2  = 20235,
};

// 20164, 31895 - Seal of Justice (custom PvP rework, as the classic fork): every landed melee hit slows the target 50% for
// 5 s (55024); the 2 s stun (20170) keeps its 5 procs per minute, with a 3 s lockout, rolled here because a procs-per-minute
// rate on the seal would gate the slow too
struct SealOfJustice : public AuraScript
{
    bool OnCheckProc(Aura* aura, ProcExecutionData& /*data*/) const override
    {
        return aura->GetEffIndex() == EFFECT_INDEX_0;
    }

    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& data) const override
    {
        Unit* caster = aura->GetTarget();
        Unit* victim = data.target;
        if (!victim || !victim->IsAlive() || victim == caster)
            return SPELL_AURA_PROC_CANT_TRIGGER;

        caster->CastSpell(victim, SPELL_SEAL_OF_JUSTICE_SLOW, TRIGGERED_OLD_TRIGGERED);

        SpellAuraHolder* holder = aura->GetHolder();
        TimePoint now = caster->GetMap()->GetCurrentClockTime();
        if (holder->IsProcReady(now) && roll_chance_f(caster->GetPPMProcChance(caster->GetAttackTime(data.attType), 5.0f)))
        {
            caster->CastSpell(victim, SPELL_SEAL_OF_JUSTICE_STUN, TRIGGERED_OLD_TRIGGERED);
            holder->SetProcCooldown(std::chrono::milliseconds(3000), now);
        }
        return SPELL_AURA_PROC_CANT_TRIGGER;
    }
};

// 5373 - Judgement of Light Intermediate
struct JudgementOfLightIntermediate : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (spell->GetTriggeredByAuraSpellInfo() == nullptr)
            return;

        uint32 triggerSpell = 0;
        switch (spell->GetTriggeredByAuraSpellInfo()->Id)
        {
            case 20185: triggerSpell = 20267; break; // Rank 1
            case 20344: triggerSpell = 20341; break; // Rank 2
            case 20345: triggerSpell = 20342; break; // Rank 3
            case 20346: triggerSpell = 20343; break; // Rank 4
            case 27162: triggerSpell = 27163; break; // Rank 5
        }
        if (triggerSpell)
            spell->GetUnitTarget()->CastSpell(nullptr, triggerSpell, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
    }
};

// 1826 - Judgement of Wisdom Intermediate
struct JudgementOfWisdomIntermediate : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (spell->GetTriggeredByAuraSpellInfo() == nullptr)
            return;

        uint32 triggerSpell = 0;
        switch (spell->GetTriggeredByAuraSpellInfo()->Id)
        {
            case 20186: triggerSpell = 20268; break; // Rank 1
            case 20354: triggerSpell = 20352; break; // Rank 2
            case 20355: triggerSpell = 20353; break; // Rank 3
            case 27164: triggerSpell = 27165; break; // Rank 4
        }
        if (triggerSpell)
            spell->GetUnitTarget()->CastSpell(nullptr, triggerSpell, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_HIDE_CAST_IN_COMBAT_LOG);
    }
};

// 31876-31878 - Judgements of the Wise (custom, was Sanctified Judgement; WotLK 3.3.5's talent): every judgement has the
// talent's chance to give Replenishment (55007) to up to 10 party or raid members within 100 yd, lowest mana first, and
// 31930's percent of base mana to the paladin
static void JudgementsOfTheWise(Unit* caster)
{
    Aura* wise = nullptr;
    for (uint32 rank : { SPELL_JUDGEMENTS_OF_THE_WISE_R3, SPELL_JUDGEMENTS_OF_THE_WISE_R2, SPELL_JUDGEMENTS_OF_THE_WISE_R1 })
        if ((wise = caster->GetAura(rank, EFFECT_INDEX_0)))
            break;
    if (!wise || !roll_chance_i(wise->GetModifier()->m_amount))
        return;

    SpellEntry const* manaSpell = sSpellTemplate.LookupEntry<SpellEntry>(SPELL_JUDGEMENTS_OF_THE_WISE_MANA);
    int32 mana = int32(caster->GetCreateMana()) * manaSpell->CalculateSimpleValue(EFFECT_INDEX_0) / 100;
    caster->CastCustomSpell(nullptr, SPELL_JUDGEMENTS_OF_THE_WISE_MANA, &mana, nullptr, nullptr, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);

    std::vector<Unit*> targets;
    Group* group = caster->GetTypeId() == TYPEID_PLAYER ? static_cast<Player*>(caster)->GetGroup() : nullptr;
    if (group)
    {
        for (GroupReference* itr = group->GetFirstMember(); itr != nullptr; itr = itr->next())
        {
            Player* member = itr->getSource();
            if (member && member->IsAlive() && member->GetPowerType() == POWER_MANA && member->IsInMap(caster) && member->IsWithinDistInMap(caster, 100.0f))
                targets.push_back(member);
        }
        std::sort(targets.begin(), targets.end(), [](Unit* a, Unit* b) { return a->GetPowerPercent(POWER_MANA) < b->GetPowerPercent(POWER_MANA); });
        if (targets.size() > 10)
            targets.resize(10);
    }
    else
        targets.push_back(caster);

    for (Unit* target : targets)
        caster->CastSpell(target, SPELL_REPLENISHMENT, TRIGGERED_OLD_TRIGGERED);
}

// 20271 - Judgement
struct spell_judgement : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        Unit* unitTarget = spell->GetUnitTarget();
        if (!unitTarget || !unitTarget->IsAlive())
            return;

        Unit* caster = spell->GetCaster();

        uint32 spellId2 = 0;

        // all seals have aura dummy
        Unit::AuraList const& m_dummyAuras = caster->GetAurasByType(SPELL_AURA_DUMMY);
        for (auto m_dummyAura : m_dummyAuras)
        {
            SpellEntry const* spellInfo = m_dummyAura->GetSpellProto();

            // search seal (all seals have judgement's aura dummy spell id in 2 effect
            if (!spellInfo || !IsSealSpell(m_dummyAura->GetSpellProto()) || m_dummyAura->GetEffIndex() != 2)
                continue;

            // must be calculated base at raw base points in spell proto, GetModifier()->m_value for S.Righteousness modified by SPELLMOD_DAMAGE
            spellId2 = m_dummyAura->GetSpellProto()->CalculateSimpleValue(EFFECT_INDEX_2);

            if (spellId2 <= 1)
                continue;

            // found, remove seal
            // custom: Righteous Judgement (talent 55019) keeps the seal up
            if (!caster->HasAura(SPELL_RIGHTEOUS_JUDGEMENT))
                caster->RemoveAurasDueToSpell(m_dummyAura->GetId());

            // custom: Sanctified Judgement's seal mana refund is gone (the talent is Judgements of the Wise now, below)
            break;
        }
        caster->CastSpell(unitTarget, spellId2, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL);
        if (spellId2 > 1)
        {
            JudgementsOfTheWise(caster);
            // custom (2026-10-09): every judgement also puts WotLK's Judgement of Wisdom effect on the target (55100). One per
            // target: the latest paladin's replaces any other, so several paladins don't multiply the mana
            unitTarget->RemoveAurasDueToSpell(SPELL_JUDGEMENT_DEBUFF);
            caster->CastSpell(unitTarget, SPELL_JUDGEMENT_DEBUFF, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_IGNORE_HIT_CALCULATION);
        }
        // custom: Judgement of Justice also interrupts, with a 3 s school lockout (the classic fork's PvP judgement)
        if (spellId2 == SPELL_JUDGEMENT_OF_JUSTICE_R1 || spellId2 == SPELL_JUDGEMENT_OF_JUSTICE_R2)
            caster->CastSpell(unitTarget, SPELL_JUDGEMENT_OF_JUSTICE_INTERRUPT, TRIGGERED_IGNORE_GCD | TRIGGERED_IGNORE_CURRENT_CASTED_SPELL | TRIGGERED_IGNORE_HIT_CALCULATION);
        if (caster->HasAura(37188)) // improved judgement
            caster->CastSpell(nullptr, 43838, TRIGGERED_OLD_TRIGGERED);

        if (caster->HasAura(40470)) // PaladinTier6Trinket
            if (roll_chance_f(50.f))
                caster->CastSpell(unitTarget, 40472, TRIGGERED_OLD_TRIGGERED);
    }
};

// 40470 - Paladin Tier 6 Trinket
struct PaladinTier6Trinket : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* /*aura*/, ProcExecutionData& procData) const override
    {
        if (!procData.spellInfo)
            return SPELL_AURA_PROC_FAILED;

        float chance = 0.f;

        // Flash of light/Holy light
        if (procData.spellInfo->SpellFamilyFlags & uint64(0x00000000C0000000))
        {
            procData.triggeredSpellId = 40471;
            chance = 15.0f;
            procData.triggerTarget = procData.victim;
        }

        if (!roll_chance_f(chance))
            return SPELL_AURA_PROC_FAILED;

        return SPELL_AURA_PROC_OK;
    }
};

// 31789 - Righteous Defense
struct RighteousDefense : public SpellScript
{
    bool OnCheckTarget(const Spell* spell, Unit* target, SpellEffectIndex /*eff*/) const override
    {
        if (target->HasFlag(UNIT_FIELD_FLAGS, UNIT_FLAG_PLAYER_CONTROLLED) || spell->GetCaster()->CanAssistSpell(target, spell->m_spellInfo))
            return true;

        return false;
    }

    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_0)
            return;

        Unit* unitTarget = spell->GetUnitTarget();
        if (!unitTarget)
            return;
        Unit* caster = spell->GetCaster();

        // non-standard cast requirement check
        if (unitTarget->getAttackers().empty())
        {
            caster->RemoveSpellCooldown(*spell->m_spellInfo, true);
            spell->SendCastResult(SPELL_FAILED_TARGET_AFFECTING_COMBAT);
            return;
        }

        // not empty (checked), copy
        Unit::AttackerSet attackers = unitTarget->getAttackers();

        // selected from list 3
        size_t size = std::min(size_t(3), attackers.size());
        for (uint32 i = 0; i < size; ++i)
        {
            Unit::AttackerSet::iterator aItr = attackers.begin();
            std::advance(aItr, urand() % attackers.size());
            caster->CastSpell((*aItr), 31790, TRIGGERED_NONE); // step 2
            attackers.erase(aItr);
        }
    }
};

enum
{
    SPELL_SEAL_OF_BLOOD_DAMAGE              = 31893,
    SPELL_SEAL_OF_BLOOD_SELF_DAMAGE         = 32221,

    SPELL_JUDGEMENT_OF_BLOOD                = 31898,
    SPELL_JUDGEMENT_OF_BLOOD_SELF_DAMAGE    = 32220
};

// 31893 - Seal of Blood
struct SealOfBloodSelfDamage : public SpellScript
{
    void OnAfterHit(Spell* spell) const override
    {
        int32 damagePoint = spell->GetTotalTargetDamage() * 10 / 100;
        spell->GetCaster()->CastCustomSpell(nullptr, SPELL_SEAL_OF_BLOOD_SELF_DAMAGE, &damagePoint, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
    }
};

// 31898 - Judgement of Blood
struct JudgementOfBloodSelfDamage : public SpellScript
{
    void OnAfterHit(Spell* spell) const override
    {
        int32 damagePoint = spell->GetTotalTargetDamage() * 33 / 100;
        spell->GetCaster()->CastCustomSpell(nullptr, SPELL_JUDGEMENT_OF_BLOOD_SELF_DAMAGE, &damagePoint, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
    }
};

// 19977 - Blessing of Light
struct BlessingOfLight : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_SPELL_HEALING_TAKEN, apply);
    }

    void OnDamageCalculate(Aura* aura, Unit* attacker, Unit* /*victim*/, int32& advertisedBenefit, float& /*totalMod*/) const override
    {
        advertisedBenefit += (aura->GetModifier()->m_amount);  // BoL is penalized since 2.3.0
        // Note: This forces the caster to keep libram equipped, but works regardless if the BOL is his or not
        if (Aura* improved = attacker->GetAura(38320, EFFECT_INDEX_0)) // improved Blessing of light
        {
            if (aura->GetEffIndex() == EFFECT_INDEX_0)
                advertisedBenefit += improved->GetModifier()->m_amount; // holy light gets full amount
            else
                advertisedBenefit += (improved->GetModifier()->m_amount / 2); // flash of light gets half
        }
    }
};

// 19752 - Divine Intervention
struct DivineIntervention : public SpellScript
{
    SpellCastResult OnCheckCast(Spell* spell, bool /*strict*/) const override
    {
        Unit* target = spell->m_targets.getUnitTarget();
        if (!target)
            return SPELL_FAILED_BAD_IMPLICIT_TARGETS;
        if (target->HasAura(23333) || target->HasAura(23335) || target->HasAura(34976)) // possibly SPELL_ATTR_EX_IMMUNITY_TO_HOSTILE_AND_FRIENDLY_EFFECTS
            return SPELL_FAILED_TARGET_AURASTATE;
        return SPELL_CAST_OK;
    }
};

// 20467, 20963, 20964, 20965, 20966, 27171 - Judgement of Command
struct JudgementOfCommand : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex /*effIdx*/) const override
    {
        if (!spell->GetUnitTarget()->IsStunned())
            spell->SetDamage(uint32(spell->GetDamage() / 2));
    }
};

// 21183, 20188, 20300, 20301, 20302, 20303, 27159 - Judgement of the Crusader
// custom: judging also deals a little Holy damage, as in the classic fork, where it is effect 3 (SCHOOL_DAMAGE, no
// spell power coefficient). It's dealt here under the judgement's own id (when this was written effect 3
// held the Replenishment trigger, which moved to Judgements of the Wise on 2026-10-08). Ranks 1-6 use classic's
// values; rank 7 (TBC only) scales rank 6 by the debuff's own growth (160 -> 218).
struct JudgementOfTheCrusader : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        if (effIdx != EFFECT_INDEX_0)
            return;

        Unit* target = spell->GetUnitTarget();
        Unit* caster = spell->GetCaster();
        if (!target || !caster || !target->IsAlive())
            return;

        uint32 minDamage, maxDamage;
        switch (spell->m_spellInfo->Id)
        {
            case 21183: minDamage = 15;  maxDamage = 15;  break; // rank 1
            case 20188: minDamage = 20;  maxDamage = 20;  break; // rank 2
            case 20300: minDamage = 22;  maxDamage = 23;  break; // rank 3
            case 20301: minDamage = 25;  maxDamage = 26;  break; // rank 4
            case 20302: minDamage = 50;  maxDamage = 52;  break; // rank 5
            case 20303: minDamage = 130; maxDamage = 138; break; // rank 6
            case 27159: minDamage = 177; maxDamage = 188; break; // rank 7
            default: return;
        }

        caster->SpellNonMeleeDamageLog(target, spell->m_spellInfo->Id, urand(minDamage, maxDamage));
        // custom: and -25% healing received (the classic fork's companion debuff; melee swings refresh it like the judgement)
        caster->CastSpell(target, SPELL_JUDGEMENT_OF_THE_CRUSADER_HEALING, TRIGGERED_OLD_TRIGGERED);
    }
};

// 55019 - Righteous Judgement (custom Retribution talent, replaces Repentance): judging keeps the seal (spell_judgement),
// seals last twice as long (effect 2, SPELLMOD_DURATION via spell_affect) and judgements deal +effect 1 % damage (here)
struct RighteousJudgement : public AuraScript
{
    void OnAuraInit(Aura* aura) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_0)
            aura->SetAffectOverriden();
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_0)
            return;
        // Judgement of Command and of Blood are melee class, the others magic
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_SPELL_DAMAGE_DONE, apply);
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_DONE, apply);
    }

    bool OnAffectCheck(Aura const* /*aura*/, SpellEntry const* spellInfo) const override
    {
        // the judgement damage spells: Righteousness 0x400, the Crusader 0x20000000 (its custom Holy hit),
        // Command / Vengeance / Blood 0x800000000, which the Seal of Vengeance 5-stack proc 42463 shares
        return spellInfo && spellInfo->SpellFamilyName == SPELLFAMILY_PALADIN && spellInfo->Id != 42463 &&
            spellInfo->IsFitToFamilyMask(uint64(0x00000820000400));
    }

    void OnDamageCalculate(Aura* aura, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        totalMod *= (100.0f + aura->GetModifier()->m_amount) / 100.0f;
    }
};

// 31846, 31847 - Warding (custom, was Spell Warding): the Protection mana source. Effect 2 (per mille): damage taken, absorbs
// included, restores that share of max health as a share of max mana, scaled; effect 3 (per mille): a block, dodge or parry
// restores that much of max mana. spell_proc_event 31846 lets every taken event through; logged as 55023.
struct Warding : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        Unit* target = aura->GetTarget();
        uint32 maxMana = target->GetMaxPower(POWER_MANA);
        int32 mana = 0;
        if (aura->GetEffIndex() == EFFECT_INDEX_1)
        {
            uint32 taken = procData.damage + procData.absorb;
            if (taken && target->GetMaxHealth())
                mana = int32(float(taken) / target->GetMaxHealth() * maxMana * aura->GetModifier()->m_amount / 1000.0f);
        }
        else if (aura->GetEffIndex() == EFFECT_INDEX_2)
        {
            if (procData.procExtra & (PROC_EX_DODGE | PROC_EX_PARRY | PROC_EX_BLOCK))
                mana = int32(maxMana * aura->GetModifier()->m_amount / 1000);
        }
        if (mana <= 0)
            return SPELL_AURA_PROC_CANT_TRIGGER;
        target->CastCustomSpell(nullptr, SPELL_WARDING_MANA, &mana, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
        return SPELL_AURA_PROC_OK;
    }
};

// 55100 - Judgement (custom, 2026-10-09: WotLK 3.3.5's Judgement of Wisdom effect, put on by every judgement): an attack or
// spell that hits the judged target has the spell's proc chance to restore effect 1's percent of the attacker's base mana,
// logged as 55101. Every attacker with mana gains, not just the paladin; seal procs count (AttributesEx3 CAN_PROC_FROM_PROCS).
struct JudgementDebuff : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& procData) const override
    {
        Unit* attacker = procData.attacker;
        if (!attacker || attacker == aura->GetTarget() || !attacker->IsAlive() || attacker->GetPowerType() != POWER_MANA)
            return SPELL_AURA_PROC_CANT_TRIGGER;
        int32 mana = int32(attacker->GetCreateMana()) * aura->GetModifier()->m_amount / 100;
        if (mana <= 0)
            return SPELL_AURA_PROC_CANT_TRIGGER;
        attacker->CastCustomSpell(nullptr, SPELL_JUDGEMENT_MANA, &mana, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
        return SPELL_AURA_PROC_OK;
    }
};

// 55007 - Replenishment (custom, WotLK's): each 1 s tick restores 1/5 of effect 1's percent of the target's max mana
struct Replenishment : public AuraScript
{
    void OnPeriodicCalculateAmount(Aura* aura, uint32& amount) const override
    {
        amount = std::max(1u, uint32(aura->GetTarget()->GetMaxPower(POWER_MANA) * aura->GetModifier()->m_amount / 500));
    }
};

// 20091, 20092 - Improved Retribution Aura (custom, as the classic fork): every 2 s in combat, the paladin's own
// Retribution Aura also deals its damage as Holy to all enemies within 8 yd (55026)
struct ImprovedRetributionAura : public AuraScript
{
    void OnPeriodicTrigger(Aura* aura, PeriodicTriggerData& data) const override
    {
        data.spellInfo = nullptr; // cast below with the aura's damage as basepoints

        Unit* target = aura->GetTarget();
        if (!target->IsAlive() || !target->IsInCombat())
            return;

        for (Aura* shield : target->GetAurasByType(SPELL_AURA_DAMAGE_SHIELD))
        {
            if (shield->GetCasterGuid() != target->GetObjectGuid() || !shield->GetSpellProto()->IsFitToFamily(SPELLFAMILY_PALADIN, uint64(0x0000000000000008)))
                continue;

            int32 damage = shield->GetModifier()->m_amount; // already includes this talent's +25/50%
            target->CastCustomSpell(nullptr, SPELL_IMPROVED_RETRIBUTION_AURA_PULSE, &damage, nullptr, nullptr, TRIGGERED_OLD_TRIGGERED);
            return;
        }
    }
};

// 32043, 35396, 35397 - Sheath of Light (custom, was Sanctified Seals; WotLK's talent): effect 1 is Holy spell damage equal to
// effect 2's percent of attack power, kept current on heartbeat
struct SheathOfLight : public AuraScript
{
    static int32 Calculate(Unit* target, SpellEntry const* spellProto)
    {
        return int32(target->GetTotalAttackPowerValue(BASE_ATTACK) * spellProto->CalculateSimpleValue(EFFECT_INDEX_1) / 100);
    }

    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        if (data.effIdx != EFFECT_INDEX_0 || !data.target)
            return value;
        return Calculate(data.target, data.spellProto);
    }

    void OnHeartbeat(Aura* aura) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_0)
            return;
        int32 amount = Calculate(aura->GetTarget(), aura->GetSpellProto());
        if (amount == aura->GetModifier()->m_amount)
            return;
        aura->ApplyModifier(false, true);
        aura->GetModifier()->m_amount = amount;
        aura->ApplyModifier(true, true);
    }
};

// 1044 - Blessing of Freedom: Divine Purpose (custom, WotLK's talent) gives it effect 3's chance to remove stuns
struct BlessingOfFreedom : public SpellScript
{
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override
    {
        Unit* target = spell->GetUnitTarget();
        if (effIdx != EFFECT_INDEX_0 || !target)
            return;
        for (uint32 rank : { SPELL_DIVINE_PURPOSE_R3, SPELL_DIVINE_PURPOSE_R2, SPELL_DIVINE_PURPOSE_R1 })
        {
            if (Aura* purpose = spell->GetCaster()->GetAura(rank, EFFECT_INDEX_2))
            {
                if (roll_chance_i(purpose->GetModifier()->m_amount))
                    target->RemoveAurasAtMechanicImmunity(convertEnumToFlag(MECHANIC_STUN), 0);
                return;
            }
        }
    }
};

// 20096-20100 - Anticipation (custom, the Cataclysm Grand Crusader idea on avoidance): a block, dodge or parry has effect 1's
// chance to reset Avenger's Shield's cooldown and make the next one free (55027). spell_proc_event 20096 limits the procs.
struct Anticipation : public AuraScript
{
    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& /*procData*/) const override
    {
        Unit* target = aura->GetTarget();
        if (aura->GetEffIndex() != EFFECT_INDEX_0 || !roll_chance_i(aura->GetModifier()->m_amount))
            return SPELL_AURA_PROC_CANT_TRIGGER;
        target->RemoveSpellCategoryCooldown(SPELL_CATEGORY_AVENGERS_SHIELD, true);
        target->CastSpell(target, SPELL_ANTICIPATION_FREE_SHIELD, TRIGGERED_OLD_TRIGGERED);
        return SPELL_AURA_PROC_CANT_TRIGGER;
    }
};

// 31850-31854 - Ardent Defender (custom): effect 1's damage reduction below 35% health is applied here, so the talent no longer
// needs the 35% aura state and stays on; at rank 5, effect 2 (a passive absorb that absorbs nothing) saves you from a killing
// blow, leaving effect 2's percent of max health, once per 2 min (55028)
struct ArdentDefender : public AuraScript
{
    void OnAuraInit(Aura* aura) const override
    {
        if (aura->GetEffIndex() == EFFECT_INDEX_0)
            aura->SetAffectOverriden();
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_0)
            return;
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_MELEE_DAMAGE_TAKEN, apply);
        aura->GetTarget()->RegisterScriptedLocationAura(aura, SCRIPT_LOCATION_SPELL_DAMAGE_TAKEN, apply);
    }

    bool OnAffectCheck(Aura const* /*aura*/, SpellEntry const* /*spellInfo*/) const override
    {
        return true;
    }

    void OnDamageCalculate(Aura* aura, Unit* /*attacker*/, Unit* /*victim*/, int32& /*advertisedBenefit*/, float& totalMod) const override
    {
        if (aura->GetTarget()->GetHealthPercent() < 35.0f)
            totalMod *= (100.0f + aura->GetModifier()->m_amount) / 100.0f;
    }

    void OnAbsorb(Aura* aura, int32& currentAbsorb, int32& /*remainingDamage*/, uint32& /*reflectedSpellId*/, int32& /*reflectDamage*/, bool& preventedDeath, bool& dropCharge, DamageEffectType /*damageType*/) const override
    {
        if (!preventedDeath && !aura->GetTarget()->HasAura(SPELL_ARDENT_DEFENDER_COOLDOWN))
            preventedDeath = true;
        dropCharge = false;
        currentAbsorb = 0; // only the death save, never an absorb
    }

    void OnAuraDeathPrevention(Aura* aura, int32& remainingDamage) const override
    {
        Unit* target = aura->GetTarget();
        SpellEntry const* cooldown = sSpellTemplate.LookupEntry<SpellEntry>(SPELL_ARDENT_DEFENDER_COOLDOWN);
        target->CastSpell(target, cooldown, TRIGGERED_OLD_TRIGGERED);
        uint32 floor = target->GetMaxHealth() * aura->GetModifier()->m_amount / 100;
        if (target->GetHealth() > floor)
            remainingDamage = target->GetHealth() - floor;
        else
        {
            remainingDamage = 0;
            target->DealHeal(target, floor - target->GetHealth(), cooldown);
        }
    }
};

// 20138-20142 - Improved Devotion Aura (custom, as the classic fork): effect 2 is armor equal to effect 3's percent of your highest
// known Devotion Aura, improved by effect 1, while your own Devotion Aura is not on you; kept current on heartbeat
struct ImprovedDevotionAura : public AuraScript
{
    static int32 Calculate(Unit* target, SpellEntry const* talent)
    {
        if (target->GetTypeId() != TYPEID_PLAYER)
            return 0;
        for (Aura* armor : target->GetAurasByType(SPELL_AURA_MOD_RESISTANCE))
            if (armor->GetCasterGuid() == target->GetObjectGuid() && armor->GetSpellProto()->IsFitToFamily(SPELLFAMILY_PALADIN, uint64(0x0000000000000040)))
                return 0; // Devotion Aura is up: its own armor applies
        for (uint32 rank : { 27149, 10293, 10292, 1032, 10291, 643, 10290, 465 }) // Devotion Aura ranks 8..1
        {
            if (!static_cast<Player*>(target)->HasSpell(rank))
                continue;
            int32 armor = sSpellTemplate.LookupEntry<SpellEntry>(rank)->CalculateSimpleValue(EFFECT_INDEX_0);
            armor = armor * (100 + talent->CalculateSimpleValue(EFFECT_INDEX_0)) / 100;
            return armor * talent->CalculateSimpleValue(EFFECT_INDEX_2) / 100;
        }
        return 0;
    }

    int32 OnAuraValueCalculate(AuraCalcData& data, int32 value) const override
    {
        if (data.effIdx != EFFECT_INDEX_1 || !data.target)
            return value;
        return Calculate(data.target, data.spellProto);
    }

    void OnHeartbeat(Aura* aura) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_1)
            return;
        int32 amount = Calculate(aura->GetTarget(), aura->GetSpellProto());
        if (amount == aura->GetModifier()->m_amount)
            return;
        aura->ApplyModifier(false, true);
        aura->GetModifier()->m_amount = amount;
        aura->ApplyModifier(true, true);
    }
};

// 20254-20256 - Improved Concentration Aura (custom, the classic fork's "under any aura"): TBC's party effect stays; while your own
// Concentration Aura is not on you, 55029 gives you its pushback resistance (35% + effect 1) and effect 2's silence/interrupt cut
struct ImprovedConcentrationAura : public AuraScript
{
    void OnApply(Aura* aura, bool apply) const override
    {
        if (!apply && aura->GetEffIndex() == EFFECT_INDEX_0)
            aura->GetTarget()->RemoveAurasDueToSpell(SPELL_IMPROVED_CONCENTRATION_PERSONAL);
    }

    void OnHeartbeat(Aura* aura) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_0)
            return;
        Unit* target = aura->GetTarget();
        if (target->GetSpellAuraHolder(SPELL_CONCENTRATION_AURA, target->GetObjectGuid()))
        {
            target->RemoveAurasDueToSpell(SPELL_IMPROVED_CONCENTRATION_PERSONAL);
            return;
        }
        if (target->HasAura(SPELL_IMPROVED_CONCENTRATION_PERSONAL))
            return;
        SpellEntry const* talent = aura->GetSpellProto();
        int32 pushback = sSpellTemplate.LookupEntry<SpellEntry>(SPELL_CONCENTRATION_AURA)->CalculateSimpleValue(EFFECT_INDEX_0) + talent->CalculateSimpleValue(EFFECT_INDEX_0);
        int32 duration = talent->CalculateSimpleValue(EFFECT_INDEX_1);
        target->CastCustomSpell(target, SPELL_IMPROVED_CONCENTRATION_PERSONAL, &pushback, &duration, &duration, TRIGGERED_OLD_TRIGGERED);
    }
};

// 31833, 31835, 31836 - Light's Grace (custom): effect 1 is TBC's (a Holy Light speeds up the next one); effect 2 makes a melee
// critical strike give 55030 (next Flash of Light or Holy Light instant). Both share the talent's 33/66/100% proc chance.
struct LightsGrace : public AuraScript
{
    bool OnCheckProc(Aura* aura, ProcExecutionData& data) const override
    {
        switch (aura->GetEffIndex())
        {
            case EFFECT_INDEX_0:
                return data.spellInfo && data.spellInfo->IsFitToFamily(SPELLFAMILY_PALADIN, uint64(0x0000000080000000)); // Holy Light
            case EFFECT_INDEX_1:
                return (data.procFlags & (PROC_FLAG_DEAL_MELEE_SWING | PROC_FLAG_DEAL_MELEE_ABILITY)) && (data.procExtra & PROC_EX_CRITICAL_HIT);
            default:
                return false;
        }
    }

    SpellAuraProcResult OnProc(Aura* aura, ProcExecutionData& /*data*/) const override
    {
        if (aura->GetEffIndex() != EFFECT_INDEX_1)
            return SPELL_AURA_PROC_OK; // effect 1: the stock trigger
        aura->GetTarget()->CastSpell(aura->GetTarget(), SPELL_LIGHTS_GRACE_INSTANT, TRIGGERED_OLD_TRIGGERED);
        return SPELL_AURA_PROC_CANT_TRIGGER;
    }
};

// 633, 2800, 10310, 27154 - Lay on Hands: Improved Lay on Hands (custom) refunds its effect 3 percent of the drained mana
struct LayOnHands : public SpellScript
{
    void OnSuccessfulFinish(Spell* spell) const override
    {
        Unit* caster = spell->GetCaster();
        for (uint32 rank : { SPELL_IMPROVED_LAY_ON_HANDS_R2, SPELL_IMPROVED_LAY_ON_HANDS_R1 })
        {
            if (Aura* improved = caster->GetAura(rank, EFFECT_INDEX_2))
            {
                if (uint32 refund = spell->GetPowerCost() * improved->GetModifier()->m_amount / 100)
                    caster->EnergizeBySpell(caster, spell->m_spellInfo, refund, POWER_MANA);
                return;
            }
        }
    }
};

// 31821 - Aura Mastery (custom, WotLK's at 5 min / 12 s): under your own Concentration Aura the party becomes immune to silence
// and interrupt (55031); any other aura of yours is re-cast while effect 2's +100% spell mod is up, and again when it ends
struct AuraMastery : public AuraScript
{
    static uint32 OwnAura(Unit* target)
    {
        for (auto const& itr : target->GetSpellAuraHolderMap())
        {
            SpellEntry const* proto = itr.second->GetSpellProto();
            if (itr.second->GetCasterGuid() == target->GetObjectGuid() && proto->Effect[EFFECT_INDEX_0] == SPELL_EFFECT_APPLY_AREA_AURA_PARTY &&
                    proto->IsFitToFamily(SPELLFAMILY_PALADIN, uint64(0x0000003004020048)))
                return proto->Id;
        }
        return 0;
    }

    static void Recast(Unit* target, uint32 auraId)
    {
        target->RemoveAurasDueToSpell(auraId);
        target->CastSpell(target, auraId, TRIGGERED_OLD_TRIGGERED);
    }

    void OnAfterApply(Aura* aura, bool apply) const override
    {
        if (!apply || aura->GetEffIndex() != EFFECT_INDEX_1)
            return;
        Unit* target = aura->GetTarget();
        uint32 own = OwnAura(target);
        if (own == SPELL_CONCENTRATION_AURA)
            target->CastSpell(target, SPELL_AURA_MASTERY_IMMUNITY, TRIGGERED_OLD_TRIGGERED);
        else if (own)
            Recast(target, own); // picks up the +100% now that it is applied
    }

    void OnApply(Aura* aura, bool apply) const override
    {
        if (apply || aura->GetEffIndex() != EFFECT_INDEX_1)
            return;
        Unit* target = aura->GetTarget();
        uint32 own = OwnAura(target);
        if (own && own != SPELL_CONCENTRATION_AURA)
            Recast(target, own); // back to normal now that the mod is gone
    }
};

// 55020 - Divine Storm (custom Retribution talent, replaces Crusader Strike): heals the paladin for 25% of the damage dealt
struct DivineStorm : public SpellScript
{
    void OnHit(Spell* spell, SpellMissInfo /*missInfo*/) const override
    {
        spell->SetScriptValue(spell->GetScriptValue() + spell->GetTotalTargetDamage());
    }

    void OnSuccessfulFinish(Spell* spell) const override
    {
        Unit* caster = spell->GetCaster();
        uint32 heal = uint32(spell->GetScriptValue() * 25 / 100);
        if (!heal || !caster->IsAlive())
            return;
        caster->DealHeal(caster, heal, sSpellTemplate.LookupEntry<SpellEntry>(SPELL_DIVINE_STORM_HEAL));
        caster->PlaySpellVisual(SPELL_VISUAL_KIT_DIVINE_STORM_HEAL);
    }
};

void LoadPaladinScripts()
{
    RegisterSpellScript<JudgementOfLightIntermediate>("spell_judgement_of_light_intermediate");
    RegisterSpellScript<JudgementOfWisdomIntermediate>("spell_judgement_of_wisdom_intermediate");
    RegisterSpellScript<DivineIntervention>("spell_divine_intervention");
    RegisterSpellScript<spell_judgement>("spell_judgement");
    RegisterSpellScript<RighteousDefense>("spell_righteous_defense");
    RegisterSpellScript<SealOfTheCrusader>("spell_seal_of_the_crusader");
    RegisterSpellScript<SealOfBloodSelfDamage>("spell_seal_of_blood_self_damage");
    RegisterSpellScript<JudgementOfBloodSelfDamage>("spell_judgement_of_blood_self_damage");
    RegisterSpellScript<PaladinTier6Trinket>("spell_paladin_tier_6_trinket");
    RegisterSpellScript<BlessingOfLight>("spell_blessing_of_light");
    RegisterSpellScript<JudgementOfCommand>("spell_judgement_of_command");
    RegisterSpellScript<SealOfVengeance>("spell_seal_of_vengeance");
    RegisterSpellScript<JudgementOfTheCrusader>("spell_judgement_of_the_crusader");
    RegisterSpellScript<RighteousJudgement>("spell_paladin_righteous_judgement");
    RegisterSpellScript<DivineStorm>("spell_paladin_divine_storm");
    RegisterSpellScript<Warding>("spell_paladin_warding");
    RegisterSpellScript<Replenishment>("spell_paladin_replenishment");
    RegisterSpellScript<SealOfJustice>("spell_paladin_seal_of_justice");
    RegisterSpellScript<JudgementDebuff>("spell_paladin_judgement_debuff");
    RegisterSpellScript<ImprovedRetributionAura>("spell_paladin_improved_retribution_aura");
    RegisterSpellScript<SheathOfLight>("spell_paladin_sheath_of_light");
    RegisterSpellScript<BlessingOfFreedom>("spell_paladin_blessing_of_freedom");
    RegisterSpellScript<Anticipation>("spell_paladin_anticipation");
    RegisterSpellScript<ArdentDefender>("spell_paladin_ardent_defender");
    RegisterSpellScript<ImprovedDevotionAura>("spell_paladin_improved_devotion_aura");
    RegisterSpellScript<ImprovedConcentrationAura>("spell_paladin_improved_concentration_aura");
    RegisterSpellScript<LightsGrace>("spell_paladin_lights_grace");
    RegisterSpellScript<LayOnHands>("spell_paladin_lay_on_hands");
    RegisterSpellScript<AuraMastery>("spell_paladin_aura_mastery");
}