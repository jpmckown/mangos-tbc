#include "Spells/Scripts/SpellScript.h"

enum LeechSpells
{
    SPELL_HEAL = 18984,

    SPELL_HEALTH_REGEN_BOOST = 55000, // instance zone aura (spell_area) carrying the leech
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

void LoadInstanceScripts() {
    RegisterSpellScript<InstanceLeechOnDamageHealing<SPELL_HEALTH_REGEN_BOOST>>("spell_instance_heal");
}
