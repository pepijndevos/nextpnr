/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2020  David Shah <dave@ds0.me>
 *
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "design_utils.h"
#include "log.h"
#include "nextpnr.h"
#include "util.h"

#include <boost/algorithm/string.hpp>
#include <queue>

NEXTPNR_NAMESPACE_BEGIN

bool Arch::pack()
{
    // attrs[id("step")] = std::string("pack");
    // archInfoToAttributes();
    // assignArchInfo();
    return true;
}

// -----------------------------------------------------------------------

void Arch::assignArchInfo()
{
    // for (auto cell : sorted(cells)) {
    //     assignCellInfo(cell.second);
    // }
}

void Arch::assignCellInfo(CellInfo *cell)
{
    // if (cell->type == id_OXIDE_COMB) {
    //     cell->lutInfo.is_memory = str_or_default(cell->params, id_MODE, "LOGIC") == "DPRAM";
    //     cell->lutInfo.is_carry = str_or_default(cell->params, id_MODE, "LOGIC") == "CCU2";
    //     cell->lutInfo.mux2_used = port_used(cell, id_OFX);
    //     cell->lutInfo.f = get_net_or_empty(cell, id_F);
    //     cell->lutInfo.ofx = get_net_or_empty(cell, id_OFX);
    // } else if (cell->type == id_OXIDE_FF) {
    //     cell->ffInfo.ctrlset.async = str_or_default(cell->params, id_SRMODE, "LSR_OVER_CE") == "ASYNC";
    //     cell->ffInfo.ctrlset.regddr_en = is_enabled(cell, id_REGDDR);
    //     cell->ffInfo.ctrlset.gsr_en = is_enabled(cell, id_GSR);
    //     cell->ffInfo.ctrlset.clkmux = id(str_or_default(cell->params, id_CLKMUX, "CLK")).index;
    //     cell->ffInfo.ctrlset.cemux = id(str_or_default(cell->params, id_CEMUX, "CE")).index;
    //     cell->ffInfo.ctrlset.lsrmux = id(str_or_default(cell->params, id_LSRMUX, "LSR")).index;
    //     cell->ffInfo.ctrlset.clk = get_net_or_empty(cell, id_CLK);
    //     cell->ffInfo.ctrlset.ce = get_net_or_empty(cell, id_CE);
    //     cell->ffInfo.ctrlset.lsr = get_net_or_empty(cell, id_LSR);
    //     cell->ffInfo.di = get_net_or_empty(cell, id_DI);
    //     cell->ffInfo.m = get_net_or_empty(cell, id_M);
    // } else if (cell->type == id_RAMW) {
    //     cell->ffInfo.ctrlset.async = true;
    //     cell->ffInfo.ctrlset.regddr_en = false;
    //     cell->ffInfo.ctrlset.gsr_en = false;
    //     cell->ffInfo.ctrlset.clkmux = id(str_or_default(cell->params, id_CLKMUX, "CLK")).index;
    //     cell->ffInfo.ctrlset.cemux = ID_CE;
    //     cell->ffInfo.ctrlset.lsrmux = ID_INV;
    //     cell->ffInfo.ctrlset.clk = get_net_or_empty(cell, id_CLK);
    //     cell->ffInfo.ctrlset.ce = nullptr;
    //     cell->ffInfo.ctrlset.lsr = get_net_or_empty(cell, id_LSR);
    //     cell->ffInfo.di = nullptr;
    //     cell->ffInfo.m = nullptr;
    // }
}

NEXTPNR_NAMESPACE_END
