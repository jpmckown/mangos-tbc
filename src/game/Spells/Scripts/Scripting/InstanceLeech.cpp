#include "Spells/Scripts/SpellScript.h"

enum LeechSpells
{
    SPELL_HEAL = 18984,

    SPELL_HEALTH_REGEN_BOOST = 55000, // instance zone aura (spell_area) carrying the leech

    // Encounter-scoped copies for outdoor world bosses (no zone aura): 30 s copies of the zone auras, refreshed by the
    // boss's pulse while in combat, so they fall off on their own after the fight. The zone auras themselves can't be
    // used there: Player::UpdateAreaDependentAuras strips them outside their spell_area zones.
    SPELL_ENCOUNTER_HEALTH_REGEN = 55010, // copy of 55000, carries the leech
    SPELL_ENCOUNTER_MANA_REGEN   = 55011, // copy of 55001
    SPELL_ENCOUNTER_COMBAT_BOOST = 55012, // copy of 55006 (40-man tier)
};

// Every bound UnitScript runs on every damage event, so each script instance must only react to
// its own aura. Binding one instance to several spell_scripts rows (or several instances to the same
// aura) would heal once per binding.
template <uint32 AuraId>
struct InstanceLeechOnDamageHealing : public UnitScript {
    void OnDealDamage(Unit* attacker, Unit* victim, uint32 damage) const override {
        if (attacker == nullptr) return; // attacker should not be null?
        if (attacker == victim) return; // no leech from self-damage (Hellfire, Seal/Judgement of Blood backlash etc.)

        bool isPet = attacker->GetOwner() && attacker->GetOwner()->GetTypeId() == TYPEID_PLAYER;
        if (!isPet && attacker->GetTypeId() != TYPEID_PLAYER) return;

        Unit* player = isPet ? attacker->GetOwner() : attacker;
        if (!player->HasAura(AuraId)) return;
        auto leech_heal = static_cast<int32>(0.05f * float(damage));
        if (leech_heal <= 0) return; // hits under 20 damage would heal 0 (combat log spam)
        if (!player->IsAlive()) return; // DoTs keep ticking after death; DealHeal would revive
        // Heal directly like Spell's health leech: no SMSG_SPELL_GO, so no cast visual/sound on every hit.
        // DealHeal still sends SMSG_SPELLHEALLOG under SPELL_HEAL for the combat text.
        if (SpellEntry const* healInfo = sSpellTemplate.LookupEntry<SpellEntry>(SPELL_HEAL))
            player->DealHeal(player, uint32(leech_heal), healInfo);
    }
};

// 55009 - Encounter Blessing (pulse cast by the boss on every enemy within 100 yd)
struct EncounterBlessingPulse : public SpellScript {
    void OnEffectExecute(Spell* spell, SpellEffectIndex effIdx) const override {
        if (effIdx != EFFECT_INDEX_0) return;
        Unit* target = spell->GetUnitTarget();
        if (!target || target->GetTypeId() != TYPEID_PLAYER || !target->IsAlive()) return;
        if (target->HasAura(SPELL_HEALTH_REGEN_BOOST)) return; // never stack with a zone blessing
        target->CastSpell(target, SPELL_ENCOUNTER_HEALTH_REGEN, TRIGGERED_OLD_TRIGGERED);
        target->CastSpell(target, SPELL_ENCOUNTER_MANA_REGEN, TRIGGERED_OLD_TRIGGERED);
        target->CastSpell(target, SPELL_ENCOUNTER_COMBAT_BOOST, TRIGGERED_OLD_TRIGGERED);
    }
};

void LoadInstanceScripts() {
    RegisterSpellScript<InstanceLeechOnDamageHealing<SPELL_HEALTH_REGEN_BOOST>>("spell_instance_heal");
    RegisterSpellScript<InstanceLeechOnDamageHealing<SPELL_ENCOUNTER_HEALTH_REGEN>>("spell_instance_heal_encounter");
    RegisterSpellScript<EncounterBlessingPulse>("spell_encounter_blessing");
}
