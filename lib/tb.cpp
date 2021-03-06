/*
  This file is part of Allie Chess.
  Copyright (C) 2018, 2019 Adam Treat

  Allie Chess is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  Allie Chess is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with Allie Chess.  If not, see <http://www.gnu.org/licenses/>.

  Additional permission under GNU GPL version 3 section 7
*/

#include "tb.h"

#include "options.h"
#include "fathom/tbprobe.h"

#include <QDebug>

class MyTB : public TB { };
Q_GLOBAL_STATIC(MyTB, TBInstance)
TB* TB::globalInstance()
{
    return TBInstance();
}

TB::TB()
    : m_enabled(false)
{
}

TB::~TB()
{
}

void TB::reset()
{
    const QString path = Options::globalInstance()->option("SyzygyPath").value();
    bool success = tb_init(path.toLatin1().constData());
    m_enabled = success && TB_LARGEST;
}

TB::Probe wdlToProbeResult(unsigned wdl)
{
    // We invert the losses and wins because Allie's nodes are from perspective of non active army
    // whereas fathom reports from the active army's perspective
    switch (wdl) {
    case TB_RESULT_FAILED:
        return TB::NotFound;
    case TB_LOSS:
        return TB::Win;
    case TB_WIN:
        return TB::Loss;
    case TB_CURSED_WIN:
    case TB_BLESSED_LOSS:
    case TB_DRAW:
        return TB::Draw;
    default:
        Q_UNREACHABLE();
        return TB::NotFound;
    }
}

Move dtzToMoveRepresentation(unsigned result)
{
    Move mv;
    mv.setStart(TB_GET_FROM(result));
    mv.setEnd(TB_GET_TO(result));

    switch (TB_GET_PROMOTES(result)) {
    case TB_PROMOTES_NONE:
        mv.setPromotion(Chess::Unknown); break;
    case TB_PROMOTES_QUEEN:
        mv.setPromotion(Chess::Queen); break;
    case TB_PROMOTES_ROOK:
        mv.setPromotion(Chess::Rook); break;
    case TB_PROMOTES_BISHOP:
        mv.setPromotion(Chess::Bishop); break;
    case TB_PROMOTES_KNIGHT:
        mv.setPromotion(Chess::Knight); break;
    }
    mv.setEnPassant(TB_GET_EP(result));
    return mv;
}

TB::Probe TB::probe(const Game &game, const Game::Position &p) const
{
    if (!m_enabled)
        return NotFound;

    if (game.halfMoveClock() != 0)
        return NotFound;

    if (p.m_hasWhiteKingCastle || p.m_hasBlackKingCastle
        || p.m_hasWhiteQueenCastle || p.m_hasBlackQueenCastle)
        return NotFound;

    if (unsigned(BitBoard(p.m_whitePositionBoard | p.m_blackPositionBoard).count()) > TB_LARGEST)
        return NotFound;

    const quint8 enpassant = !p.m_enPassantTarget.isValid() ? 0 : p.m_enPassantTarget.data();

    const unsigned result = tb_probe_wdl(
        p.m_whitePositionBoard.data(),
        p.m_blackPositionBoard.data(),
        p.m_kingsBoard.data(),
        p.m_queensBoard.data(),
        p.m_rooksBoard.data(),
        p.m_bishopsBoard.data(),
        p.m_knightsBoard.data(),
        p.m_pawnsBoard.data(),
        0 /*half move clock*/,
        0 /*castling rights*/,
        enpassant,
        p.m_activeArmy == Chess::White);
    return wdlToProbeResult(result);
}

TB::Probe TB::probeDTZ(const Game &game, const Game::Position &p, Move *suggestedMove,
    int *dtz) const
{
    if (!m_enabled)
        return NotFound;

    if (p.m_hasWhiteKingCastle || p.m_hasBlackKingCastle
        || p.m_hasWhiteQueenCastle || p.m_hasBlackQueenCastle)
        return NotFound;

    if (unsigned(BitBoard(p.m_whitePositionBoard | p.m_blackPositionBoard).count()) > TB_LARGEST)
        return NotFound;

    const quint8 enpassant = !p.m_enPassantTarget.isValid() ? 0 : p.m_enPassantTarget.data();

    const unsigned result = tb_probe_root(
        p.m_whitePositionBoard.data(),
        p.m_blackPositionBoard.data(),
        p.m_kingsBoard.data(),
        p.m_queensBoard.data(),
        p.m_rooksBoard.data(),
        p.m_bishopsBoard.data(),
        p.m_knightsBoard.data(),
        p.m_pawnsBoard.data(),
        unsigned(game.halfMoveClock()),
        0 /*castling rights*/,
        enpassant,
        p.m_activeArmy == Chess::White,
        nullptr /*alternative results*/);

    switch (result) {
    case TB_RESULT_FAILED:
    case TB_RESULT_CHECKMATE:
    case TB_RESULT_STALEMATE:
        return NotFound;
    }

    *dtz = TB_GET_DTZ(result);
    *suggestedMove = dtzToMoveRepresentation(result);
    return wdlToProbeResult(TB_GET_WDL(result));
}
