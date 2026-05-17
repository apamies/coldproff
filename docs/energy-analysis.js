/*
 * ColdProff Energy Analysis
 * Calculates actual power consumption over 7 days
 */

// ============= COMPONENT SPECIFICATIONS =============

const components = {
    BC660K_GL: {
        name: "BC660K-GL NB-IoT SoC",
        psm_sleep: { current_mA: 0.0008, duration_s: 10799 },  // 800nA for 3h - 2s
        active_qeng: { current_mA: 80, duration_s: 2 },        // ~80mA for 2s (AT+QENG read)
    },
    TMP117: {
        name: "TMP117 Temperature Sensor",
        continuous: { current_mA: 0.1, duration_s: 1 },        // 100µA for 1s read
    },
    ST25DV64K: {
        name: "ST25DV64K NFC + EEPROM",
        i2c_write: { current_mA: 0.05, duration_s: 0.05 },     // ~50µA for 50ms write
    }
};

// ============= MEASUREMENT CYCLE TIMELINE =============

function calculateCycleConsumption() {
    console.log("=== ENERGY CONSUMPTION PER CYCLE (3 hours) ===\n");

    // PSM Sleep (dominant component)
    const psm_energy = components.BC660K_GL.psm_sleep.current_mA *
                      (components.BC660K_GL.psm_sleep.duration_s / 3600);
    console.log(`PSM Sleep (800nA × 10799s):\n  ${psm_energy.toFixed(4)} mAh`);

    // BC660K Active - AT+QENG read
    const qeng_energy = components.BC660K_GL.active_qeng.current_mA *
                       (components.BC660K_GL.active_qeng.duration_s / 3600);
    console.log(`  AT+QENG read (80mA × 2s):\n  ${qeng_energy.toFixed(4)} mAh`);

    // TMP117 read
    const temp_energy = components.TMP117.continuous.current_mA *
                       (components.TMP117.continuous.duration_s / 3600);
    console.log(`TMP117 read (100µA × 1s):\n  ${temp_energy.toFixed(6)} mAh`);

    // ST25DV64K I2C write
    const nfc_energy = components.ST25DV64K.i2c_write.current_mA *
                      (components.ST25DV64K.i2c_write.duration_s / 3600);
    console.log(`ST25DV64K write (50µA × 50ms):\n  ${nfc_energy.toFixed(6)} mAh\n`);

    const total_per_cycle = psm_energy + qeng_energy + temp_energy + nfc_energy;
    console.log(`📊 Total per cycle: ${total_per_cycle.toFixed(4)} mAh`);
    console.log(`   (PSM dominates at ${(psm_energy/total_per_cycle*100).toFixed(1)}%)\n`);

    return total_per_cycle;
}

function calculate7Days(energy_per_cycle) {
    const cycles = 56;  // 7 days × 24h / 3h per cycle
    const total_7days = cycles * energy_per_cycle;

    console.log("=== 7-DAY PROJECTION ===\n");
    console.log(`Cycles in 7 days: ${cycles}`);
    console.log(`Total consumption: ${total_7days.toFixed(2)} mAh\n`);

    return total_7days;
}

function recommendBatteries(energy_required) {
    console.log("=== BATTERY OPTIONS ===\n");

    const batteries = [
        { model: "LR44 / LR1154", capacity: 120, diameter: 11.6, thickness: 5.4, cost: 0.08 },
        { model: "LR1120", capacity: 180, diameter: 11.6, thickness: 6.0, cost: 0.10 },
        { model: "LR1130", capacity: 80, diameter: 11.6, thickness: 5.4, cost: 0.07 },
        { model: "LR43", capacity: 100, diameter: 11.6, thickness: 4.2, cost: 0.08 },
        { model: "LR936 / SR936", capacity: 70, diameter: 9.5, thickness: 3.6, cost: 0.06 },
        { model: "SR44 (Silver)", capacity: 160, diameter: 11.6, thickness: 5.4, cost: 0.15 },
        { model: "CR2032 (baseline)", capacity: 220, diameter: 20.0, thickness: 3.2, cost: 0.18 },
    ];

    const safety_margin = 10;  // 10x safety margin
    const min_required = energy_required * safety_margin;

    console.log(`Required capacity (10x safety margin): ${min_required.toFixed(2)} mAh\n`);

    const viable = [];

    batteries.forEach(bat => {
        const margin = bat.capacity / energy_required;
        const safety_ok = bat.capacity >= min_required;
        const status = safety_ok ? "✅ VIABLE" : safety_ok ? "⚠️ MARGINAL" : "❌ TOO SMALL";

        console.log(`${bat.model}`);
        console.log(`  Capacity: ${bat.capacity} mAh | Size: ${bat.diameter}×${bat.thickness}mm`);
        console.log(`  Safety margin: ${margin.toFixed(1)}x | Cost: €${bat.cost.toFixed(2)} | ${status}`);

        if (safety_ok) {
            viable.push({...bat, margin});
        }
        console.log();
    });

    return viable;
}

// ============= MAIN CALCULATION =============

const energy_per_cycle = calculateCycleConsumption();
const total_7days = calculate7Days(energy_per_cycle);
const viable_batteries = recommendBatteries(total_7days);

console.log("\n=== RECOMMENDATION ===\n");
if (viable_batteries.length > 0) {
    const best = viable_batteries.reduce((a, b) => a.capacity < b.capacity ? a : b);
    console.log(`🏆 BEST CHOICE: ${best.model}`);
    console.log(`   Capacity: ${best.capacity} mAh (${best.margin.toFixed(1)}x safety margin)`);
    console.log(`   Size: ${best.diameter}×${best.thickness}mm (vs CR2032: 20×3.2mm)`);
    console.log(`   Cost: €${best.cost.toFixed(2)} (save €${(0.18 - best.cost).toFixed(2)} per unit)`);
    console.log(`   BOM impact: Smaller, cheaper, same functionality ✓`);
}
