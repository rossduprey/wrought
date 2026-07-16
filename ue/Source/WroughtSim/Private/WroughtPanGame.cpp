#include "WroughtPanGame.h"

// The core lives here and nowhere else in the module's public surface. Everything
// wrought:: is confined to this translation unit.
#include "pan_game.h"

using wrought::PanGame;

void UWroughtPanGame::StartPan(int32 Deposit)
{
    if (!Game)
    {
        Game = new PanGame();
    }
    Game->begin(static_cast<int>(Deposit));
    Accumulator = 0.f;
    PumpAndBroadcast();
}

void UWroughtPanGame::Swirl()      { if (Game) { Game->swirl();      PumpAndBroadcast(); } }
void UWroughtPanGame::PickStone()  { if (Game) { Game->pick_stone(); PumpAndBroadcast(); } }
void UWroughtPanGame::RepanTub()   { if (Game) { Game->repan_tub();  PumpAndBroadcast(); } }
void UWroughtPanGame::NewDeposit() { if (Game) { Game->new_deposit();PumpAndBroadcast(); } }
void UWroughtPanGame::Keep()       { if (Game) { Game->keep();       PumpAndBroadcast(); } }

void UWroughtPanGame::Advance(float DeltaSeconds)
{
    if (!Game || !Game->running())
    {
        return;
    }

    // The sim is authored at a fixed step. Accumulate real time and take whole
    // PAN_DT steps; never scale one step by a variable frame dt.
    Accumulator += DeltaSeconds;
    // Guard against a huge hitch (e.g. a breakpoint) spiralling the loop.
    const float MaxCatchUp = static_cast<float>(wrought::PAN_DT) * 20.f;
    if (Accumulator > MaxCatchUp)
    {
        Accumulator = MaxCatchUp;
    }

    bool bStepped = false;
    while (Accumulator >= static_cast<float>(wrought::PAN_DT) && Game->running())
    {
        Game->tick();
        Accumulator -= static_cast<float>(wrought::PAN_DT);
        bStepped = true;
    }

    if (bStepped)
    {
        PumpAndBroadcast();
    }
}

bool UWroughtPanGame::IsPanning() const
{
    return Game != nullptr && Game->running();
}

FWroughtPanState UWroughtPanGame::GetState() const
{
    FWroughtPanState State;
    if (!Game)
    {
        return State;
    }

    State.bRunning      = Game->running();
    State.TimeSeconds   = static_cast<float>(Game->time_s());
    State.CutFraction   = static_cast<float>(Game->cut_fraction());
    State.PanMassGrams  = static_cast<float>(Game->pan_mass_g());
    State.BlackFraction = static_cast<float>(Game->black_frac());
    State.Colour        = UTF8_TO_TCHAR(Game->colour());
    State.bHasStones    = Game->has_stones();
    State.TubMassGrams  = static_cast<float>(Game->tub_mass_g());
    State.bTubGlitters  = Game->tub_glitters();

    for (const PanGame::AssayRow& R : Game->assay_rows())
    {
        FWroughtPanRow Row;
        Row.Phase = UTF8_TO_TCHAR(R.id);
        Row.Grams = static_cast<float>(R.grams);
        Row.Grade = static_cast<float>(R.grade);
        Row.Kept  = static_cast<float>(R.kept);
        Row.Arrow = (R.arrow == '^') ? TEXT("^") : (R.arrow == 'v') ? TEXT("v") : TEXT("");
        Row.bGone = R.gone;
        State.Rows.Add(Row);
    }
    return State;
}

void UWroughtPanGame::PumpAndBroadcast()
{
    if (!Game)
    {
        return;
    }
    for (const std::string& Line : Game->drain_feedback())
    {
        OnPanFeedback.Broadcast(FString(UTF8_TO_TCHAR(Line.c_str())));
    }
    OnPanState.Broadcast(GetState());
}

void UWroughtPanGame::BeginDestroy()
{
    delete Game;
    Game = nullptr;
    Super::BeginDestroy();
}
