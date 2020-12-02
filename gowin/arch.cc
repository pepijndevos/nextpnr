/*
 *  nextpnr -- Next Generation Place and Route
 *
 *  Copyright (C) 2018  Clifford Wolf <clifford@symbioticeda.com>
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

#include <iostream>
#include <math.h>
#include "embed.h"
#include "nextpnr.h"
#include "placer1.h"
#include "placer_heap.h"
#include "router1.h"
#include "router2.h"
#include "util.h"

NEXTPNR_NAMESPACE_BEGIN

WireInfo &Arch::wire_info(IdString wire)
{
    auto w = wires.find(wire);
    if (w == wires.end())
        NPNR_ASSERT_FALSE_STR("no wire named " + wire.str(this));
    return w->second;
}

PipInfo &Arch::pip_info(IdString pip)
{
    auto p = pips.find(pip);
    if (p == pips.end())
        NPNR_ASSERT_FALSE_STR("no pip named " + pip.str(this));
    return p->second;
}

BelInfo &Arch::bel_info(IdString bel)
{
    auto b = bels.find(bel);
    if (b == bels.end())
        NPNR_ASSERT_FALSE_STR("no bel named " + bel.str(this));
    return b->second;
}

void Arch::addWire(IdString name, IdString type, int x, int y)
{
    // std::cout << name.str(this) << std::endl;
    NPNR_ASSERT(wires.count(name) == 0);
    WireInfo &wi = wires[name];
    wi.name = name;
    wi.type = type;
    wi.x = x;
    wi.y = y;

    wire_ids.push_back(name);
}

void Arch::addPip(IdString name, IdString type, IdString srcWire, IdString dstWire, DelayInfo delay, Loc loc)
{
    NPNR_ASSERT(pips.count(name) == 0);
    PipInfo &pi = pips[name];
    pi.name = name;
    pi.type = type;
    pi.srcWire = srcWire;
    pi.dstWire = dstWire;
    pi.delay = delay;
    pi.loc = loc;

    wire_info(srcWire).downhill.push_back(name);
    wire_info(dstWire).uphill.push_back(name);
    pip_ids.push_back(name);

    if (int(tilePipDimZ.size()) <= loc.x)
        tilePipDimZ.resize(loc.x + 1);

    if (int(tilePipDimZ[loc.x].size()) <= loc.y)
        tilePipDimZ[loc.x].resize(loc.y + 1);

    gridDimX = std::max(gridDimX, loc.x + 1);
    gridDimY = std::max(gridDimY, loc.x + 1);
    tilePipDimZ[loc.x][loc.y] = std::max(tilePipDimZ[loc.x][loc.y], loc.z + 1);
}

void Arch::addBel(IdString name, IdString type, Loc loc, bool gb)
{
    NPNR_ASSERT(bels.count(name) == 0);
    NPNR_ASSERT(bel_by_loc.count(loc) == 0);
    BelInfo &bi = bels[name];
    bi.name = name;
    bi.type = type;
    bi.x = loc.x;
    bi.y = loc.y;
    bi.z = loc.z;
    bi.gb = gb;

    bel_ids.push_back(name);
    bel_by_loc[loc] = name;

    if (int(bels_by_tile.size()) <= loc.x)
        bels_by_tile.resize(loc.x + 1);

    if (int(bels_by_tile[loc.x].size()) <= loc.y)
        bels_by_tile[loc.x].resize(loc.y + 1);

    bels_by_tile[loc.x][loc.y].push_back(name);

    if (int(tileBelDimZ.size()) <= loc.x)
        tileBelDimZ.resize(loc.x + 1);

    if (int(tileBelDimZ[loc.x].size()) <= loc.y)
        tileBelDimZ[loc.x].resize(loc.y + 1);

    gridDimX = std::max(gridDimX, loc.x + 1);
    gridDimY = std::max(gridDimY, loc.x + 1);
    tileBelDimZ[loc.x][loc.y] = std::max(tileBelDimZ[loc.x][loc.y], loc.z + 1);
}

void Arch::addBelInput(IdString bel, IdString name, IdString wire)
{
    NPNR_ASSERT(bel_info(bel).pins.count(name) == 0);
    PinInfo &pi = bel_info(bel).pins[name];
    pi.name = name;
    pi.wire = wire;
    pi.type = PORT_IN;

    wire_info(wire).downhill_bel_pins.push_back(BelPin{bel, name});
    wire_info(wire).bel_pins.push_back(BelPin{bel, name});
}

void Arch::addBelOutput(IdString bel, IdString name, IdString wire)
{
    NPNR_ASSERT(bel_info(bel).pins.count(name) == 0);
    PinInfo &pi = bel_info(bel).pins[name];
    pi.name = name;
    pi.wire = wire;
    pi.type = PORT_OUT;

    wire_info(wire).uphill_bel_pin = BelPin{bel, name};
    wire_info(wire).bel_pins.push_back(BelPin{bel, name});
}

void Arch::addBelInout(IdString bel, IdString name, IdString wire)
{
    NPNR_ASSERT(bel_info(bel).pins.count(name) == 0);
    PinInfo &pi = bel_info(bel).pins[name];
    pi.name = name;
    pi.wire = wire;
    pi.type = PORT_INOUT;

    wire_info(wire).downhill_bel_pins.push_back(BelPin{bel, name});
    wire_info(wire).bel_pins.push_back(BelPin{bel, name});
}

void Arch::addGroupBel(IdString group, IdString bel) { groups[group].bels.push_back(bel); }

void Arch::addGroupWire(IdString group, IdString wire) { groups[group].wires.push_back(wire); }

void Arch::addGroupPip(IdString group, IdString pip) { groups[group].pips.push_back(pip); }

void Arch::addGroupGroup(IdString group, IdString grp) { groups[group].groups.push_back(grp); }

void Arch::addDecalGraphic(DecalId decal, const GraphicElement &graphic)
{
    decal_graphics[decal].push_back(graphic);
    refreshUi();
}

void Arch::setWireDecal(WireId wire, DecalXY decalxy)
{
    wire_info(wire).decalxy = decalxy;
    refreshUiWire(wire);
}

void Arch::setPipDecal(PipId pip, DecalXY decalxy)
{
    pip_info(pip).decalxy = decalxy;
    refreshUiPip(pip);
}

void Arch::setBelDecal(BelId bel, DecalXY decalxy)
{
    bel_info(bel).decalxy = decalxy;
    refreshUiBel(bel);
}

void Arch::setGroupDecal(GroupId group, DecalXY decalxy)
{
    groups[group].decalxy = decalxy;
    refreshUiGroup(group);
}

void Arch::setWireAttr(IdString wire, IdString key, const std::string &value) { wire_info(wire).attrs[key] = value; }

void Arch::setPipAttr(IdString pip, IdString key, const std::string &value) { pip_info(pip).attrs[key] = value; }

void Arch::setBelAttr(IdString bel, IdString key, const std::string &value) { bel_info(bel).attrs[key] = value; }

void Arch::setLutK(int K) { args.K = K; }

void Arch::setDelayScaling(double scale, double offset)
{
    args.delayScale = scale;
    args.delayOffset = offset;
}

void Arch::addCellTimingClock(IdString cell, IdString port) { cellTiming[cell].portClasses[port] = TMG_CLOCK_INPUT; }

void Arch::addCellTimingDelay(IdString cell, IdString fromPort, IdString toPort, DelayInfo delay)
{
    if (get_or_default(cellTiming[cell].portClasses, fromPort, TMG_IGNORE) == TMG_IGNORE)
        cellTiming[cell].portClasses[fromPort] = TMG_COMB_INPUT;
    if (get_or_default(cellTiming[cell].portClasses, toPort, TMG_IGNORE) == TMG_IGNORE)
        cellTiming[cell].portClasses[toPort] = TMG_COMB_OUTPUT;
    cellTiming[cell].combDelays[CellDelayKey{fromPort, toPort}] = delay;
}

void Arch::addCellTimingSetupHold(IdString cell, IdString port, IdString clock, DelayInfo setup, DelayInfo hold)
{
    TimingClockingInfo ci;
    ci.clock_port = clock;
    ci.edge = RISING_EDGE;
    ci.setup = setup;
    ci.hold = hold;
    cellTiming[cell].clockingInfo[port].push_back(ci);
    cellTiming[cell].portClasses[port] = TMG_REGISTER_INPUT;
}

void Arch::addCellTimingClockToOut(IdString cell, IdString port, IdString clock, DelayInfo clktoq)
{
    TimingClockingInfo ci;
    ci.clock_port = clock;
    ci.edge = RISING_EDGE;
    ci.clockToQ = clktoq;
    cellTiming[cell].clockingInfo[port].push_back(ci);
    cellTiming[cell].portClasses[port] = TMG_REGISTER_OUTPUT;
}

// ---------------------------------------------------------------

// TODO represent wires more intelligently.
IdString Arch::wireToGlobal(int &row, int &col, const DatabasePOD *db, IdString &wire)
{
    std::string wirename = wire.str(this);
    char buf[32];
    if (wirename == "VCC" || wirename == "GND") {
        return wire;
    }
    if (!isdigit(wirename[1]) || !isdigit(wirename[2]) || !isdigit(wirename[3])) {
        snprintf(buf, 32, "R%dC%d_%s", row + 1, col + 1, wirename.c_str());
        return id(buf);
    }
    char direction = wirename[0];
    int num = std::stoi(wirename.substr(1, 2));
    int segment = std::stoi(wirename.substr(3, 1));
    switch (direction) {
    case 'N':
        row += segment;
        break;
    case 'S':
        row -= segment;
        break;
    case 'E':
        col -= segment;
        break;
    case 'W':
        col += segment;
        break;
    default:
        snprintf(buf, 32, "R%dC%d_%s", row + 1, col + 1, wirename.c_str());
        return id(buf);
        break;
    }
    // wires wrap around the edges
    // assumes 0-based indexes
    if (row < 0) {
        row = -1 - row;
        direction = 'N';
    } else if (col < 0) {
        col = -1 - col;
        direction = 'W';
    } else if (row >= db->rows) {
        row = 2 * db->rows - 1 - row;
        direction = 'S';
    } else if (col >= db->cols) {
        col = 2 * db->cols - 1 - col;
        direction = 'E';
    }
    snprintf(buf, 32, "%c%d0", direction, num);
    wire = id(buf);
    snprintf(buf, 32, "R%dC%d_%c%d", row + 1, col + 1, direction, num);
    return id(buf);
}

const PairPOD* pairLookup(const PairPOD *list, const size_t len, const int src, const int dest)
{
    for (size_t i = 0; i < len; i++) {
        const PairPOD *pair = &list[i];
        if ((src < 0 || pair->src_id == src) && (dest < 0 || pair->dest_id == dest)) {
            return pair;
        }
    }
    return nullptr;
}

bool destCompare (PairPOD i,PairPOD j) { return (i.dest_id<j.dest_id); }
bool aliasCompare (GlobalAliasPOD i, GlobalAliasPOD j) {
    return (i.dest_row<j.dest_row) ||
           (i.dest_row==j.dest_row && i.dest_col<j.dest_col) ||
           (i.dest_row==j.dest_row && i.dest_col==j.dest_col && i.dest_id<j.dest_id);
}

const GlobalAliasPOD* aliasLookup(const GlobalAliasPOD *first, int len, const GlobalAliasPOD val)
{
    auto res = std::lower_bound(first, first+len, val, aliasCompare);
    if (res-first != len && !aliasCompare(val, *res)) {
        return res;
    } else {
        return nullptr;
    }
}

Arch::Arch(ArchArgs args) : args(args)
{
    family = "GW1N-9";
    device = "GW1N-9";
    speed = "C6/E5";   // or whatever
    package = "QFN48"; // or something

    // Load database
    std::string chipdb = stringf("gowin/chipdb-%s.bin", family.c_str());
    auto db = reinterpret_cast<const DatabasePOD *>(get_chipdb(chipdb));
    if (db == nullptr)
        log_error("Failed to load chipdb '%s'\n", chipdb.c_str());
    if (db->family.get() != family) {
        log_error("Database is for family '%s' but provided device is family '%s'.\n", db->family.get(),
                  family.c_str());
    }
    // setup id strings
    for (size_t i = 0; i < db->num_ids; i++) {
        IdString::initialize_add(this, db->id_strs[i].get(), uint32_t(i) + db->num_constids);
    }
    // setup db
    char buf[32];
    for (int i = 0; i < db->rows * db->cols; i++) {
        int row = i / db->cols;
        int col = i % db->cols;
        const TilePOD *tile = db->grid[i].get();
        // setup wires
        const PairPOD* pips[2] = { tile->pips.get(), tile->clock_pips.get()};
        unsigned int num_pips[2] = { tile->num_pips, tile->num_clock_pips};
        for(int p = 0; p < 2; p++) {
            for (unsigned int j = 0; j < num_pips[p]; j++) {
                const PairPOD pip = pips[p][j];
                int destrow = row;
                int destcol = col;
                IdString destid = pip.dest_id;
                IdString gdestname = wireToGlobal(destrow, destcol, db, destid);
                if (wires.count(gdestname) == 0)
                    addWire(gdestname, pip.dest_id, destcol, destrow);
                int srcrow = row;
                int srccol = col;
                IdString srcid = pip.src_id;
                IdString gsrcname = wireToGlobal(srcrow, srccol, db, srcid);
                if (wires.count(gsrcname) == 0)
                    addWire(gsrcname, pip.src_id, srccol, srcrow);
            }
        }
        for (unsigned int j = 0; j < tile->num_bels; j++) {
            const BelsPOD *bel = &tile->bels[j];
            IdString belname;
            IdString wirename;
            IdString portname;
            int z = 0;
            bool dff = true;
            switch (static_cast<ConstIds>(bel->type_id)) {
            // fall through the ++
            case ID_LUT7:
                z++;
                dff = false;
            case ID_LUT6:
                z++;
                dff = false;
            case ID_LUT5:
                z++;
            case ID_LUT4:
                z++;
            case ID_LUT3:
                z++;
            case ID_LUT2:
                z++;
            case ID_LUT1:
                z++;
            case ID_LUT0:
                // common LUT+DFF code
                snprintf(buf, 32, "R%dC%d_SLICE%d", row + 1, col + 1, z);
                belname = id(buf);
                addBel(belname, id_SLICE, Loc(col, row, z), false);
                snprintf(buf, 32, "R%dC%d_F%d", row + 1, col + 1, z);
                wirename = id(buf);
                addBelOutput(belname, id_F, wirename);
                snprintf(buf, 32, "R%dC%d_A%d", row + 1, col + 1, z);
                wirename = id(buf);
                addBelInput(belname, id_A, wirename);
                snprintf(buf, 32, "R%dC%d_B%d", row + 1, col + 1, z);
                wirename = id(buf);
                addBelInput(belname, id_B, wirename);
                snprintf(buf, 32, "R%dC%d_C%d", row + 1, col + 1, z);
                wirename = id(buf);
                addBelInput(belname, id_C, wirename);
                snprintf(buf, 32, "R%dC%d_D%d", row + 1, col + 1, z);
                wirename = id(buf);
                addBelInput(belname, id_D, wirename);
                if (dff) {
                    snprintf(buf, 32, "R%dC%d_CLK%d", row + 1, col + 1, z / 2);
                    wirename = id(buf);
                    addBelInput(belname, id_CLK, wirename);
                    snprintf(buf, 32, "R%dC%d_LSR%d", row + 1, col + 1, z / 2);
                    wirename = id(buf);
                    addBelInput(belname, id_LSR, wirename);
                    snprintf(buf, 32, "R%dC%d_CE%d", row + 1, col + 1, z / 2);
                    wirename = id(buf);
                    addBelInput(belname, id_CE, wirename);
                    snprintf(buf, 32, "R%dC%d_Q%d", row + 1, col + 1, z);
                    wirename = id(buf);
                    addBelOutput(belname, id_Q, wirename);
                }
                break;
            case ID_IOBJ:
                z++;
            case ID_IOBI:
                z++;
            case ID_IOBH:
                z++;
            case ID_IOBG:
                z++;
            case ID_IOBF:
                z++;
            case ID_IOBE:
                z++;
            case ID_IOBD:
                z++;
            case ID_IOBC:
                z++;
            case ID_IOBB:
                z++;
            case ID_IOBA:
                snprintf(buf, 32, "R%dC%d_IOB%c", row + 1, col + 1, 'A' + z);
                belname = id(buf);
                addBel(belname, id_IOB, Loc(col, row, z), false);
                portname = pairLookup(bel->ports.get(), bel->num_ports, -1, ID_O)->src_id;
                snprintf(buf, 32, "R%dC%d_%s", row + 1, col + 1, portname.c_str(this));
                wirename = id(buf);
                addBelInput(belname, id_I, wirename);
                portname = pairLookup(bel->ports.get(), bel->num_ports, -1, ID_O)->src_id;
                snprintf(buf, 32, "R%dC%d_%s", row + 1, col + 1, portname.c_str(this));
                wirename = id(buf);
                addBelInput(belname, id_O, wirename);
                portname = pairLookup(bel->ports.get(), bel->num_ports, -1, ID_OE)->src_id;
                snprintf(buf, 32, "R%dC%d_%s", row + 1, col + 1, portname.c_str(this));
                wirename = id(buf);
                addBelInput(belname, id_OEN, wirename);
                break;

            default:
                break;
            }
        }
    }
    // setup pips
    for (int i = 0; i < db->rows * db->cols; i++) {
        int row = i / db->cols;
        int col = i % db->cols;
        const TilePOD *tile = db->grid[i].get();
        const PairPOD* pips[2] = { tile->pips.get(), tile->clock_pips.get()};
        unsigned int num_pips[2] = { tile->num_pips, tile->num_clock_pips};
        for(int p = 0; p < 2; p++) {
            for (unsigned int j = 0; j < num_pips[p]; j++) {
                const PairPOD pip = pips[p][j];
                int destrow = row;
                int destcol = col;
                IdString destid = pip.dest_id;
                IdString gdestname = wireToGlobal(destrow, destcol, db, destid);
                int srcrow = row;
                int srccol = col;
                IdString srcid = pip.src_id;
                IdString gsrcname = wireToGlobal(srcrow, srccol, db, srcid);

                snprintf(buf, 32, "R%dC%d_%s_%s", row + 1, col + 1, IdString(pip.src_id).c_str(this),
                        IdString(pip.dest_id).c_str(this));
                IdString pipname = id(buf);
                DelayInfo delay;
                delay.delay = 0.1; // TODO
                // local alias
                auto local_alias = pairLookup(tile->aliases.get(), tile->num_aliases, -1, srcid.index);
                // std::cout << "srcid " << srcid.str(this) << std::endl;
                if(local_alias!=nullptr) {
                    srcid = local_alias->src_id;
                    gsrcname = wireToGlobal(srcrow, srccol, db, srcid);
                }
                // global alias
                srcid = pip.src_id;
                GlobalAliasPOD alias;
                alias.dest_col = srccol;
                alias.dest_row = srcrow;
                alias.dest_id = srcid.index;
                auto alias_src = aliasLookup(db->aliases.get(), db->num_aliases, alias);
                if(alias_src!=nullptr) {
                    srccol = alias_src->src_col;
                    srcrow = alias_src->src_row;
                    srcid = alias_src->src_id;
                    gsrcname = wireToGlobal(srcrow, srccol, db, srcid);
                    //std::cout << buf << std::endl;
                }
                addPip(pipname, pip.dest_id, gsrcname, gdestname, delay, Loc(col, row, j));
            }
        }
    }
    // Dummy for empty decals
    decal_graphics[IdString()];
}

void IdString::initialize_arch(const BaseCtx *ctx)
{
#define X(t) initialize_add(ctx, #t, ID_##t);
#include "constids.inc"
#undef X
}

// ---------------------------------------------------------------

BelId Arch::getBelByName(IdString name) const
{
    if (bels.count(name))
        return name;
    return BelId();
}

IdString Arch::getBelName(BelId bel) const { return bel; }

Loc Arch::getBelLocation(BelId bel) const
{
    auto &info = bels.at(bel);
    return Loc(info.x, info.y, info.z);
}

BelId Arch::getBelByLocation(Loc loc) const
{
    auto it = bel_by_loc.find(loc);
    if (it != bel_by_loc.end())
        return it->second;
    return BelId();
}

const std::vector<BelId> &Arch::getBelsByTile(int x, int y) const { return bels_by_tile.at(x).at(y); }

bool Arch::getBelGlobalBuf(BelId bel) const { return bels.at(bel).gb; }

uint32_t Arch::getBelChecksum(BelId bel) const
{
    // FIXME
    return 0;
}

void Arch::bindBel(BelId bel, CellInfo *cell, PlaceStrength strength)
{
    bels.at(bel).bound_cell = cell;
    cell->bel = bel;
    cell->belStrength = strength;
    refreshUiBel(bel);
}

void Arch::unbindBel(BelId bel)
{
    bels.at(bel).bound_cell->bel = BelId();
    bels.at(bel).bound_cell->belStrength = STRENGTH_NONE;
    bels.at(bel).bound_cell = nullptr;
    refreshUiBel(bel);
}

bool Arch::checkBelAvail(BelId bel) const { return bels.at(bel).bound_cell == nullptr; }

CellInfo *Arch::getBoundBelCell(BelId bel) const { return bels.at(bel).bound_cell; }

CellInfo *Arch::getConflictingBelCell(BelId bel) const { return bels.at(bel).bound_cell; }

const std::vector<BelId> &Arch::getBels() const { return bel_ids; }

IdString Arch::getBelType(BelId bel) const { return bels.at(bel).type; }

const std::map<IdString, std::string> &Arch::getBelAttrs(BelId bel) const { return bels.at(bel).attrs; }

WireId Arch::getBelPinWire(BelId bel, IdString pin) const
{
    const auto &bdata = bels.at(bel);
    if (!bdata.pins.count(pin))
        log_error("bel '%s' has no pin '%s'\n", bel.c_str(this), pin.c_str(this));
    return bdata.pins.at(pin).wire;
}

PortType Arch::getBelPinType(BelId bel, IdString pin) const { return bels.at(bel).pins.at(pin).type; }

std::vector<IdString> Arch::getBelPins(BelId bel) const
{
    std::vector<IdString> ret;
    for (auto &it : bels.at(bel).pins)
        ret.push_back(it.first);
    return ret;
}

// ---------------------------------------------------------------

WireId Arch::getWireByName(IdString name) const
{
    if (wires.count(name))
        return name;
    return WireId();
}

IdString Arch::getWireName(WireId wire) const { return wire; }

IdString Arch::getWireType(WireId wire) const { return wires.at(wire).type; }

const std::map<IdString, std::string> &Arch::getWireAttrs(WireId wire) const { return wires.at(wire).attrs; }

uint32_t Arch::getWireChecksum(WireId wire) const
{
    // FIXME
    return 0;
}

void Arch::bindWire(WireId wire, NetInfo *net, PlaceStrength strength)
{
    wires.at(wire).bound_net = net;
    net->wires[wire].pip = PipId();
    net->wires[wire].strength = strength;
    refreshUiWire(wire);
}

void Arch::unbindWire(WireId wire)
{
    auto &net_wires = wires.at(wire).bound_net->wires;

    auto pip = net_wires.at(wire).pip;
    if (pip != PipId()) {
        pips.at(pip).bound_net = nullptr;
        refreshUiPip(pip);
    }

    net_wires.erase(wire);
    wires.at(wire).bound_net = nullptr;
    refreshUiWire(wire);
}

bool Arch::checkWireAvail(WireId wire) const { return wires.at(wire).bound_net == nullptr; }

NetInfo *Arch::getBoundWireNet(WireId wire) const { return wires.at(wire).bound_net; }

NetInfo *Arch::getConflictingWireNet(WireId wire) const { return wires.at(wire).bound_net; }

const std::vector<BelPin> &Arch::getWireBelPins(WireId wire) const { return wires.at(wire).bel_pins; }

const std::vector<WireId> &Arch::getWires() const { return wire_ids; }

// ---------------------------------------------------------------

PipId Arch::getPipByName(IdString name) const
{
    if (pips.count(name))
        return name;
    return PipId();
}

IdString Arch::getPipName(PipId pip) const { return pip; }

IdString Arch::getPipType(PipId pip) const { return pips.at(pip).type; }

const std::map<IdString, std::string> &Arch::getPipAttrs(PipId pip) const { return pips.at(pip).attrs; }

uint32_t Arch::getPipChecksum(PipId wire) const
{
    // FIXEDME?
    return wire.index;
}

void Arch::bindPip(PipId pip, NetInfo *net, PlaceStrength strength)
{
    WireId wire = pips.at(pip).dstWire;
    pips.at(pip).bound_net = net;
    wires.at(wire).bound_net = net;
    net->wires[wire].pip = pip;
    net->wires[wire].strength = strength;
    refreshUiPip(pip);
    refreshUiWire(wire);
}

void Arch::unbindPip(PipId pip)
{
    WireId wire = pips.at(pip).dstWire;
    wires.at(wire).bound_net->wires.erase(wire);
    pips.at(pip).bound_net = nullptr;
    wires.at(wire).bound_net = nullptr;
    refreshUiPip(pip);
    refreshUiWire(wire);
}

bool Arch::checkPipAvail(PipId pip) const { return pips.at(pip).bound_net == nullptr; }

NetInfo *Arch::getBoundPipNet(PipId pip) const { return pips.at(pip).bound_net; }

NetInfo *Arch::getConflictingPipNet(PipId pip) const { return pips.at(pip).bound_net; }

WireId Arch::getConflictingPipWire(PipId pip) const { return pips.at(pip).bound_net ? pips.at(pip).dstWire : WireId(); }

const std::vector<PipId> &Arch::getPips() const { return pip_ids; }

Loc Arch::getPipLocation(PipId pip) const { return pips.at(pip).loc; }

WireId Arch::getPipSrcWire(PipId pip) const { return pips.at(pip).srcWire; }

WireId Arch::getPipDstWire(PipId pip) const { return pips.at(pip).dstWire; }

DelayInfo Arch::getPipDelay(PipId pip) const { return pips.at(pip).delay; }

const std::vector<PipId> &Arch::getPipsDownhill(WireId wire) const { return wires.at(wire).downhill; }

const std::vector<PipId> &Arch::getPipsUphill(WireId wire) const { return wires.at(wire).uphill; }

// ---------------------------------------------------------------

GroupId Arch::getGroupByName(IdString name) const { return name; }

IdString Arch::getGroupName(GroupId group) const { return group; }

std::vector<GroupId> Arch::getGroups() const
{
    std::vector<GroupId> ret;
    for (auto &it : groups)
        ret.push_back(it.first);
    return ret;
}

const std::vector<BelId> &Arch::getGroupBels(GroupId group) const { return groups.at(group).bels; }

const std::vector<WireId> &Arch::getGroupWires(GroupId group) const { return groups.at(group).wires; }

const std::vector<PipId> &Arch::getGroupPips(GroupId group) const { return groups.at(group).pips; }

const std::vector<GroupId> &Arch::getGroupGroups(GroupId group) const { return groups.at(group).groups; }

// ---------------------------------------------------------------

delay_t Arch::estimateDelay(WireId src, WireId dst) const
{
    const WireInfo &s = wires.at(src);
    const WireInfo &d = wires.at(dst);
    int dx = abs(s.x - d.x);
    int dy = abs(s.y - d.y);
    return (dx + dy) * args.delayScale + args.delayOffset;
}

delay_t Arch::predictDelay(const NetInfo *net_info, const PortRef &sink) const
{
    const auto &driver = net_info->driver;
    auto driver_loc = getBelLocation(driver.cell->bel);
    auto sink_loc = getBelLocation(sink.cell->bel);

    int dx = abs(sink_loc.x - driver_loc.x);
    int dy = abs(sink_loc.y - driver_loc.y);
    return (dx + dy) * args.delayScale + args.delayOffset;
}

bool Arch::getBudgetOverride(const NetInfo *net_info, const PortRef &sink, delay_t &budget) const { return false; }

ArcBounds Arch::getRouteBoundingBox(WireId src, WireId dst) const
{
    ArcBounds bb;

    int src_x = wires.at(src).x;
    int src_y = wires.at(src).y;
    int dst_x = wires.at(dst).x;
    int dst_y = wires.at(dst).y;

    bb.x0 = src_x;
    bb.y0 = src_y;
    bb.x1 = src_x;
    bb.y1 = src_y;

    auto extend = [&](int x, int y) {
        bb.x0 = std::min(bb.x0, x);
        bb.x1 = std::max(bb.x1, x);
        bb.y0 = std::min(bb.y0, y);
        bb.y1 = std::max(bb.y1, y);
    };
    extend(dst_x, dst_y);
    return bb;
}

// ---------------------------------------------------------------

bool Arch::place()
{
    std::string placer = str_or_default(settings, id("placer"), defaultPlacer);
    if (placer == "heap") {
        bool have_iobuf_or_constr = false;
        for (auto cell : sorted(cells)) {
            CellInfo *ci = cell.second;
            if (ci->type == id("IOB") || ci->bel != BelId() || ci->attrs.count(id("BEL"))) {
                have_iobuf_or_constr = true;
                break;
            }
        }
        bool retVal;
        if (!have_iobuf_or_constr) {
            log_warning("Unable to use HeAP due to a lack of IO buffers or constrained cells as anchors; reverting to "
                        "SA.\n");
            retVal = placer1(getCtx(), Placer1Cfg(getCtx()));
        } else {
            PlacerHeapCfg cfg(getCtx());
            cfg.ioBufTypes.insert(id("IOB"));
            retVal = placer_heap(getCtx(), cfg);
        }
        getCtx()->settings[getCtx()->id("place")] = 1;
        archInfoToAttributes();
        return retVal;
    } else if (placer == "sa") {
        bool retVal = placer1(getCtx(), Placer1Cfg(getCtx()));
        getCtx()->settings[getCtx()->id("place")] = 1;
        archInfoToAttributes();
        return retVal;
    } else {
        log_error("Generic architecture does not support placer '%s'\n", placer.c_str());
    }
}

bool Arch::route()
{
    std::string router = str_or_default(settings, id("router"), defaultRouter);
    bool result;
    if (router == "router1") {
        result = router1(getCtx(), Router1Cfg(getCtx()));
    } else if (router == "router2") {
        router2(getCtx(), Router2Cfg(getCtx()));
        result = true;
    } else {
        log_error("iCE40 architecture does not support router '%s'\n", router.c_str());
    }
    getCtx()->settings[getCtx()->id("route")] = 1;
    archInfoToAttributes();
    return result;
}

// ---------------------------------------------------------------

const std::vector<GraphicElement> &Arch::getDecalGraphics(DecalId decal) const
{
    if (!decal_graphics.count(decal)) {
        std::cerr << "No decal named " << decal.str(this) << std::endl;
        log_error("No decal named %s!\n", decal.c_str(this));
    }
    return decal_graphics.at(decal);
}

DecalXY Arch::getBelDecal(BelId bel) const { return bels.at(bel).decalxy; }

DecalXY Arch::getWireDecal(WireId wire) const { return wires.at(wire).decalxy; }

DecalXY Arch::getPipDecal(PipId pip) const { return pips.at(pip).decalxy; }

DecalXY Arch::getGroupDecal(GroupId group) const { return groups.at(group).decalxy; }

// ---------------------------------------------------------------

bool Arch::getCellDelay(const CellInfo *cell, IdString fromPort, IdString toPort, DelayInfo &delay) const
{
    if (!cellTiming.count(cell->name))
        return false;
    const auto &tmg = cellTiming.at(cell->name);
    auto fnd = tmg.combDelays.find(CellDelayKey{fromPort, toPort});
    if (fnd != tmg.combDelays.end()) {
        delay = fnd->second;
        return true;
    } else {
        return false;
    }
}

// Get the port class, also setting clockPort if applicable
TimingPortClass Arch::getPortTimingClass(const CellInfo *cell, IdString port, int &clockInfoCount) const
{
    if (!cellTiming.count(cell->name))
        return TMG_IGNORE;
    const auto &tmg = cellTiming.at(cell->name);
    if (tmg.clockingInfo.count(port))
        clockInfoCount = int(tmg.clockingInfo.at(port).size());
    else
        clockInfoCount = 0;
    return get_or_default(tmg.portClasses, port, TMG_IGNORE);
}

TimingClockingInfo Arch::getPortClockingInfo(const CellInfo *cell, IdString port, int index) const
{
    NPNR_ASSERT(cellTiming.count(cell->name));
    const auto &tmg = cellTiming.at(cell->name);
    NPNR_ASSERT(tmg.clockingInfo.count(port));
    return tmg.clockingInfo.at(port).at(index);
}

bool Arch::isValidBelForCell(CellInfo *cell, BelId bel) const
{
    std::vector<const CellInfo *> cells;
    cells.push_back(cell);
    Loc loc = getBelLocation(bel);
    for (auto tbel : getBelsByTile(loc.x, loc.y)) {
        if (tbel == bel)
            continue;
        CellInfo *bound = getBoundBelCell(tbel);
        if (bound != nullptr)
            cells.push_back(bound);
    }
    return cellsCompatible(cells.data(), int(cells.size()));
}

bool Arch::isBelLocationValid(BelId bel) const
{
    std::vector<const CellInfo *> cells;
    Loc loc = getBelLocation(bel);
    for (auto tbel : getBelsByTile(loc.x, loc.y)) {
        CellInfo *bound = getBoundBelCell(tbel);
        if (bound != nullptr)
            cells.push_back(bound);
    }
    return cellsCompatible(cells.data(), int(cells.size()));
}

#ifdef WITH_HEAP
const std::string Arch::defaultPlacer = "heap";
#else
const std::string Arch::defaultPlacer = "sa";
#endif

const std::vector<std::string> Arch::availablePlacers = {"sa",
#ifdef WITH_HEAP
                                                         "heap"
#endif
};

const std::string Arch::defaultRouter = "router1";
const std::vector<std::string> Arch::availableRouters = {"router1", "router2"};

void Arch::assignArchInfo()
{
    for (auto &cell : getCtx()->cells) {
        CellInfo *ci = cell.second.get();
        if (ci->type == id("SLICE")) {
            ci->is_slice = true;
            ci->slice_clk = get_net_or_empty(ci, id("CLK"));
        } else {
            ci->is_slice = false;
        }
        ci->user_group = int_or_default(ci->attrs, id("PACK_GROUP"), -1);
    }
}

bool Arch::cellsCompatible(const CellInfo **cells, int count) const
{
    const NetInfo *clk = nullptr;
    int group = -1;
    for (int i = 0; i < count; i++) {
        const CellInfo *ci = cells[i];
        if (ci->is_slice && ci->slice_clk != nullptr) {
            if (clk == nullptr)
                clk = ci->slice_clk;
            else if (clk != ci->slice_clk)
                return false;
        }
        if (ci->user_group != -1) {
            if (group == -1)
                group = ci->user_group;
            else if (group != ci->user_group)
                return false;
        }
    }
    return true;
}

NEXTPNR_NAMESPACE_END
