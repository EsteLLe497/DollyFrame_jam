#pragma once

#include <string_view>

enum class EntityTag
{
    Unknown,
    Player,
    Enemy,
    PhotoBox,
    Goal,
    Checkpoint,
    PhotoSource,
    Hazard,
    Bullet,
    DropItem,
    Battery,
    Log,
    BatterySwitch,
    Elevator,
    DamagePlatform,
    DamagePlatformSpike,
    LaserSwitch,
    Shutter,
    ProtectiveWall,
    LaserTurret,
    LaserBeam,
    MarkerLight,
    StageLight,
    SepiaRubble,
    SepiaElevator,
    Filter,
    Barrel,
    FallingRock,
    JumpPad,
    Shield,
    BossShield,
    Boss1Shield,
    MidBoss1Shield,
    CapturedShield,
    WalkerMeleeAttack,
    BossShockwave,
    MidBoss3Fist,
    Merchant,
};

inline const char* EntityTagToString(EntityTag tag)
{
    switch (tag)
    {
    case EntityTag::Player: return "Player";
    case EntityTag::Enemy: return "Enemy";
    case EntityTag::PhotoBox: return "PhotoBox";
    case EntityTag::Goal: return "Goal";
    case EntityTag::Checkpoint: return "Checkpoint";
    case EntityTag::PhotoSource: return "PhotoSource";
    case EntityTag::Hazard: return "Hazard";
    case EntityTag::Bullet: return "Bullet";
    case EntityTag::DropItem: return "DropItem";
    case EntityTag::Battery: return "Battery";
    case EntityTag::Log: return "Log";
    case EntityTag::BatterySwitch: return "BatterySwitch";
    case EntityTag::Elevator: return "Elevator";
    case EntityTag::DamagePlatform: return "DamagePlatform";
    case EntityTag::DamagePlatformSpike: return "DamagePlatformSpike";
    case EntityTag::LaserSwitch: return "LaserSwitch";
    case EntityTag::Shutter: return "Shutter";
    case EntityTag::ProtectiveWall: return "ProtectiveWall";
    case EntityTag::LaserTurret: return "LaserTurret";
    case EntityTag::LaserBeam: return "LaserBeam";
    case EntityTag::MarkerLight: return "MarkerLight";
    case EntityTag::StageLight: return "StageLight";
    case EntityTag::SepiaRubble: return "SepiaRubble";
    case EntityTag::SepiaElevator: return "SepiaElevator";
    case EntityTag::Filter: return "Filter";
    case EntityTag::Barrel: return "Barrel";
    case EntityTag::FallingRock: return "FallingRock";
    case EntityTag::JumpPad: return "JumpPad";
    case EntityTag::Shield: return "Shield";
    case EntityTag::BossShield: return "BossShield";
    case EntityTag::Boss1Shield: return "Boss1Shield";
    case EntityTag::MidBoss1Shield: return "MidBoss1Shield";
    case EntityTag::CapturedShield: return "CapturedShield";
    case EntityTag::WalkerMeleeAttack: return "WalkerMeleeAttack";
    case EntityTag::BossShockwave: return "BossShockwave";
    case EntityTag::MidBoss3Fist: return "MidBoss3Fist";
    case EntityTag::Merchant: return "Merchant";
    default: return "";
    }
}

inline EntityTag EntityTagFromString(std::string_view tag)
{
    if (tag == "Player") return EntityTag::Player;
    if (tag == "Enemy") return EntityTag::Enemy;
    if (tag == "PhotoBox") return EntityTag::PhotoBox;
    if (tag == "Goal") return EntityTag::Goal;
    if (tag == "Checkpoint") return EntityTag::Checkpoint;
    if (tag == "PhotoSource") return EntityTag::PhotoSource;
    if (tag == "Hazard") return EntityTag::Hazard;
    if (tag == "Bullet") return EntityTag::Bullet;
    if (tag == "DropItem") return EntityTag::DropItem;
    if (tag == "Battery") return EntityTag::Battery;
    if (tag == "Log") return EntityTag::Log;
    if (tag == "BatterySwitch") return EntityTag::BatterySwitch;
    if (tag == "Elevator") return EntityTag::Elevator;
    if (tag == "DamagePlatform") return EntityTag::DamagePlatform;
    if (tag == "DamagePlatformSpike") return EntityTag::DamagePlatformSpike;
    if (tag == "LaserSwitch") return EntityTag::LaserSwitch;
    if (tag == "Shutter") return EntityTag::Shutter;
    if (tag == "ProtectiveWall") return EntityTag::ProtectiveWall;
    if (tag == "LaserTurret") return EntityTag::LaserTurret;
    if (tag == "LaserBeam") return EntityTag::LaserBeam;
    if (tag == "MarkerLight") return EntityTag::MarkerLight;
    if (tag == "StageLight") return EntityTag::StageLight;
    if (tag == "SepiaRubble") return EntityTag::SepiaRubble;
    if (tag == "SepiaElevator") return EntityTag::SepiaElevator;
    if (tag == "Filter") return EntityTag::Filter;
    if (tag == "Barrel") return EntityTag::Barrel;
    if (tag == "FallingRock") return EntityTag::FallingRock;
    if (tag == "JumpPad") return EntityTag::JumpPad;
    if (tag == "Shield") return EntityTag::Shield;
    if (tag == "BossShield") return EntityTag::BossShield;
    if (tag == "Boss1Shield") return EntityTag::Boss1Shield;
    if (tag == "MidBoss1Shield") return EntityTag::MidBoss1Shield;
    if (tag == "CapturedShield") return EntityTag::CapturedShield;
    if (tag == "WalkerMeleeAttack") return EntityTag::WalkerMeleeAttack;
    if (tag == "BossShockwave") return EntityTag::BossShockwave;
    if (tag == "MidBoss3Fist") return EntityTag::MidBoss3Fist;
    if (tag == "Merchant") return EntityTag::Merchant;
    return EntityTag::Unknown;
}
