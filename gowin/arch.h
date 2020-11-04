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

#ifndef NEXTPNR_H
#error Include "arch.h" via "nextpnr.h" only.
#endif

#include <boost/iostreams/device/mapped_file.hpp>

#include <iostream>

NEXTPNR_NAMESPACE_BEGIN

template <typename T> struct RelPtr
{
    int32_t offset;

    // void set(const T *ptr) {
    //     offset = reinterpret_cast<const char*>(ptr) -
    //              reinterpret_cast<const char*>(this);
    // }

    const T *get() const { return reinterpret_cast<const T *>(reinterpret_cast<const char *>(this) + offset); }

    const T &operator[](size_t index) const { return get()[index]; }

    const T &operator*() const { return *(get()); }

    const T *operator->() const { return get(); }
};

/*
    Fully deduplicated database

    There are two key data structures in the database:

    Locations (aka tile but not called this to avoid confusion
    with Lattice terminology), are a (x, y) location.

    Local wires; pips and bels are all stored once per variety of location
    (called a location type) with a separate grid containing the location type
    at a (x, y) coordinate.

    Each location also has _neighbours_, other locations with interconnected
    wires. The set of neighbours for a location are called a _neighbourhood_.

    Each variety of _neighbourhood_ for a location type is also stored once,
    using relative coordinates.

*/

NPNR_PACKED_STRUCT(struct RelWireInfoPOD {
    int16_t rel_x, rel_y;
    uint16_t wire_index;
    uint8_t loc_type;
    uint8_t arc_flags;
});

NPNR_PACKED_STRUCT(struct PipInfoPOD {
    uint16_t from_wire, to_wire;
    uint16_t flags;
    uint16_t padding;
    int32_t tile_type;
});

NPNR_PACKED_STRUCT(struct LocWireInfoPOD {
    int32_t name; // wire name in tile IdString
    uint32_t flags;
    int32_t num_uphill, num_downhill, num_bpins;
    // Note this pip lists exclude neighbourhood pips
    // RelPtr<int32_t> pips_uh, pips_dh; // list of uphill/downhill pip indices in tile
    // RelPtr<BelPinPOD> bel_pins;
});

NPNR_PACKED_STRUCT(struct BelInfoPOD {
    int32_t name;             // bel name in tile IdString
    int32_t type;             // bel type IdString
    int16_t rel_x, rel_y;     // bel location relative to parent
    int32_t z;                // bel location absolute Z
    //RelPtr<BelWirePOD> ports; // ports, sorted by name IdString
    int32_t num_ports;        // number of ports
});

NPNR_PACKED_STRUCT(struct LocTypePOD {
    uint32_t num_bels, num_wires, num_pips, num_nhtypes;
    RelPtr<BelInfoPOD> bels;
    RelPtr<LocWireInfoPOD> wires;
    RelPtr<PipInfoPOD> pips;
    //RelPtr<LocNeighourhoodPOD> neighbourhoods;
});

NPNR_PACKED_STRUCT(struct IdStringDBPOD {
    uint32_t num_file_ids; // number of IDs loaded from constids.inc
    uint32_t num_bba_ids;  // number of IDs in BBA file
    RelPtr<RelPtr<char>> bba_id_strs;
});

NPNR_PACKED_STRUCT(struct ChipInfoPOD {
    RelPtr<char> device_name;
    uint16_t width;
    uint16_t height;
    uint32_t num_tiles;
    uint32_t num_pads;
    uint32_t num_packages;
});

NPNR_PACKED_STRUCT(struct DatabasePOD {
    uint32_t version;
    uint32_t num_chips;
    uint32_t num_loctypes;
    RelPtr<char> family;
    RelPtr<ChipInfoPOD> chips;
    RelPtr<LocTypePOD> loctypes;
    RelPtr<IdStringDBPOD> ids;
});

// -----------------------------------------------------------------------

// Helper functions for database access
namespace {
template <typename Id> const LocTypePOD &chip_loc_data(const DatabasePOD *db, const ChipInfoPOD *chip, const Id &id)
{
    // return db->loctypes[chip->grid[id.tile].loc_type];
}

// template <typename Id>
// const LocNeighourhoodPOD &chip_nh_data(const DatabasePOD *db, const ChipInfoPOD *chip, const Id &id)
// {
//     auto &t = chip->grid[id.tile];
//     return db->loctypes[t.loc_type].neighbourhoods[t.neighbourhood_type];
// }

inline const BelInfoPOD &chip_bel_data(const DatabasePOD *db, const ChipInfoPOD *chip, BelId id)
{
    return chip_loc_data(db, chip, id).bels[id.index];
}
inline const LocWireInfoPOD &chip_wire_data(const DatabasePOD *db, const ChipInfoPOD *chip, WireId id)
{
    return chip_loc_data(db, chip, id).wires[id.index];
}
inline const PipInfoPOD &chip_pip_data(const DatabasePOD *db, const ChipInfoPOD *chip, PipId id)
{
    return chip_loc_data(db, chip, id).pips[id.index];
}
inline bool chip_rel_tile(const ChipInfoPOD *chip, int32_t base, int16_t rel_x, int16_t rel_y, int32_t &next)
{
    int32_t curr_x = base % chip->width;
    int32_t curr_y = base / chip->width;
    int32_t new_x = curr_x + rel_x;
    int32_t new_y = curr_y + rel_y;
    if (new_x < 0 || new_x >= chip->width)
        return false;
    if (new_y < 0 || new_y >= chip->height)
        return false;
    next = new_y * chip->width + new_x;
    return true;
}
inline int32_t chip_tile_from_xy(const ChipInfoPOD *chip, int32_t x, int32_t y) { return y * chip->width + x; }
inline bool chip_get_branch_loc(const ChipInfoPOD *chip, int32_t x, int32_t &branch_x)
{
}
inline bool chip_get_spine_loc(const ChipInfoPOD *chip, int32_t x, int32_t y, int32_t &spine_x, int32_t &spine_y)
{
}
inline bool chip_get_hrow_loc(const ChipInfoPOD *chip, int32_t x, int32_t y, int32_t &hrow_x, int32_t &hrow_y)
{
}
inline bool chip_branch_tile(const ChipInfoPOD *chip, int32_t x, int32_t y, int32_t &next)
{
    int32_t branch_x;
    if (!chip_get_branch_loc(chip, x, branch_x))
        return false;
    next = chip_tile_from_xy(chip, branch_x, y);
    return true;
}
inline bool chip_rel_loc_tile(const ChipInfoPOD *chip, int32_t base, const RelWireInfoPOD &rel, int32_t &next)
{
}
inline WireId chip_canonical_wire(const DatabasePOD *db, const ChipInfoPOD *chip, int32_t tile, uint16_t index)
{
}
inline bool chip_wire_is_primary(const DatabasePOD *db, const ChipInfoPOD *chip, int32_t tile, uint16_t index)
{
}
} // namespace

// -----------------------------------------------------------------------

const int bba_version =
#include "bba_version.inc"
        ;

struct ArchArgs
{
    std::string device;
};

struct Arch : BaseCtx
{
    ArchArgs args;
    std::string family, device, package, speed, rating, variant;
    Arch(ArchArgs args);

    // -------------------------------------------------

    // Database references
    boost::iostreams::mapped_file_source blob_file;
    const DatabasePOD *db;
    const ChipInfoPOD *chip_info;

    int package_idx;

    // Binding states
    struct LogicTileStatus
    {
        struct SliceStatus
        {
            bool valid = true, dirty = true;
        } slices[4];
        struct HalfTileStatus
        {
            bool valid = true, dirty = true;
        } halfs[2];
        CellInfo *cells[32];
    };

    struct TileStatus
    {
        std::vector<CellInfo *> boundcells;
        LogicTileStatus *lts = nullptr;
        ~TileStatus() { delete lts; }
    };

    std::vector<TileStatus> tileStatus;
    std::unordered_map<WireId, NetInfo *> wire_to_net;
    std::unordered_map<PipId, NetInfo *> pip_to_net;

    // -------------------------------------------------

    std::string getChipName() const;

    IdString archId() const { return id("gowin"); }
    ArchArgs archArgs() const { return args; }
    IdString archArgsToId(ArchArgs args) const;

    int getGridDimX() const { return chip_info->width; }
    int getGridDimY() const { return chip_info->height; }
    int getTileBelDimZ(int, int) const { return 256; }
    int getTilePipDimZ(int, int) const { return 1; }

    // -------------------------------------------------

    BelId getBelByName(IdString name) const;

    IdString getBelName(BelId bel) const
    {
        std::string name = "X";
        name += std::to_string(bel.tile % chip_info->width);
        name += "/Y";
        name += std::to_string(bel.tile / chip_info->width);
        name += "/";
        name += nameOf(IdString(bel_data(bel).name));
        return id(name);
    }

    uint32_t getBelChecksum(BelId bel) const { return (bel.tile << 16) ^ bel.index; }

    void bindBel(BelId bel, CellInfo *cell, PlaceStrength strength)
    {
        NPNR_ASSERT(bel != BelId());
        NPNR_ASSERT(tileStatus[bel.tile].boundcells[bel.index] == nullptr);
        tileStatus[bel.tile].boundcells[bel.index] = cell;
        cell->bel = bel;
        cell->belStrength = strength;
        refreshUiBel(bel);

        // if (tile_is(bel, LOC_LOGIC))
        //     update_logic_bel(bel, cell);
    }

    void unbindBel(BelId bel)
    {
        NPNR_ASSERT(bel != BelId());
        NPNR_ASSERT(tileStatus[bel.tile].boundcells[bel.index] != nullptr);

        // if (tile_is(bel, LOC_LOGIC))
        //     update_logic_bel(bel, nullptr);

        tileStatus[bel.tile].boundcells[bel.index]->bel = BelId();
        tileStatus[bel.tile].boundcells[bel.index]->belStrength = STRENGTH_NONE;
        tileStatus[bel.tile].boundcells[bel.index] = nullptr;
        refreshUiBel(bel);
    }

    bool checkBelAvail(BelId bel) const
    {
        NPNR_ASSERT(bel != BelId());
        return tileStatus[bel.tile].boundcells[bel.index] == nullptr;
    }

    CellInfo *getBoundBelCell(BelId bel) const
    {
        NPNR_ASSERT(bel != BelId());
        return tileStatus[bel.tile].boundcells[bel.index];
    }

    CellInfo *getConflictingBelCell(BelId bel) const
    {
        NPNR_ASSERT(bel != BelId());
        return tileStatus[bel.tile].boundcells[bel.index];
    }

    std::vector<BelId> getBels() const
    {
        // BelRange range;
        // range.b.cursor_tile = 0;
        // range.b.cursor_index = -1;
        // range.b.chip = chip_info;
        // range.b.db = db;
        // ++range.b; //-1 and then ++ deals with the case of no bels in the first tile
        // range.e.cursor_tile = chip_info->width * chip_info->height;
        // range.e.cursor_index = 0;
        // range.e.chip = chip_info;
        // range.e.db = db;
        // return range;
    }

    Loc getBelLocation(BelId bel) const
    {
        NPNR_ASSERT(bel != BelId());
        Loc loc;
        loc.x = bel.tile % chip_info->width;
        loc.y = bel.tile / chip_info->width;
        loc.z = bel_data(bel).z;
        return loc;
    }

    BelId getBelByLocation(Loc loc) const
    {
        // BelId ret;
        // auto &t = db->loctypes[chip_info->grid[loc.y * chip_info->width + loc.x].loc_type];
        // if (loc.x >= 0 && loc.x < chip_info->width && loc.y >= 0 && loc.y < chip_info->height) {
        //     for (size_t i = 0; i < t.num_bels; i++) {
        //         if (t.bels[i].z == loc.z) {
        //             ret.tile = loc.y * chip_info->width + loc.x;
        //             ret.index = i;
        //             break;
        //         }
        //     }
        // }
        // return ret;
    }

    std::vector<BelId> getBelsByTile(int x, int y) const;

    bool getBelGlobalBuf(BelId bel) const { return false; }

    IdString getBelType(BelId bel) const
    {
        NPNR_ASSERT(bel != BelId());
        return IdString(bel_data(bel).type);
    }

    std::vector<std::pair<IdString, std::string>> getBelAttrs(BelId bel) const;

    WireId getBelPinWire(BelId bel, IdString pin) const;
    PortType getBelPinType(BelId bel, IdString pin) const;
    std::vector<IdString> getBelPins(BelId bel) const;

    // -------------------------------------------------

    WireId getWireByName(IdString name) const;
    IdString getWireName(WireId wire) const
    {
        std::string name = "X";
        name += std::to_string(wire.tile % chip_info->width);
        name += "/Y";
        name += std::to_string(wire.tile / chip_info->width);
        name += "/";
        name += nameOf(IdString(wire_data(wire).name));
        return id(name);
    }

    IdString getWireType(WireId wire) const;
    std::vector<std::pair<IdString, std::string>> getWireAttrs(WireId wire) const;

    uint32_t getWireChecksum(WireId wire) const { return (wire.tile << 16) ^ wire.index; }

    void bindWire(WireId wire, NetInfo *net, PlaceStrength strength)
    {
        NPNR_ASSERT(wire != WireId());
        NPNR_ASSERT(wire_to_net[wire] == nullptr);
        wire_to_net[wire] = net;
        net->wires[wire].pip = PipId();
        net->wires[wire].strength = strength;
        refreshUiWire(wire);
    }

    void unbindWire(WireId wire)
    {
        NPNR_ASSERT(wire != WireId());
        NPNR_ASSERT(wire_to_net[wire] != nullptr);

        auto &net_wires = wire_to_net[wire]->wires;
        auto it = net_wires.find(wire);
        NPNR_ASSERT(it != net_wires.end());

        auto pip = it->second.pip;
        if (pip != PipId()) {
            pip_to_net[pip] = nullptr;
        }

        net_wires.erase(it);
        wire_to_net[wire] = nullptr;
        refreshUiWire(wire);
    }

    bool checkWireAvail(WireId wire) const
    {
        NPNR_ASSERT(wire != WireId());
        auto w2n = wire_to_net.find(wire);
        return w2n == wire_to_net.end() || w2n->second == nullptr;
    }

    NetInfo *getBoundWireNet(WireId wire) const
    {
        NPNR_ASSERT(wire != WireId());
        auto w2n = wire_to_net.find(wire);
        return w2n == wire_to_net.end() ? nullptr : w2n->second;
    }

    NetInfo *getConflictingWireNet(WireId wire) const
    {
        NPNR_ASSERT(wire != WireId());
        auto w2n = wire_to_net.find(wire);
        return w2n == wire_to_net.end() ? nullptr : w2n->second;
    }

    WireId getConflictingWireWire(WireId wire) const { return wire; }

    DelayInfo getWireDelay(WireId wire) const
    {
        DelayInfo delay;
        delay.min_raise_delay = 0;
        delay.max_raise_delay = 0;
        delay.min_fall_delay = 0;
        delay.max_fall_delay = 0;
        return delay;
    }

    std::vector<BelPin> getWireBelPins(WireId wire) const
    {
        // WireBelPinRange range;
        // NPNR_ASSERT(wire != WireId());
        // NeighWireRange nwr = neigh_wire_range(wire);
        // range.b.chip = chip_info;
        // range.b.db = db;
        // range.b.twi = nwr.b;
        // range.b.twi_end = nwr.e;
        // range.b.cursor = -1;
        // ++range.b;
        // range.e.chip = chip_info;
        // range.e.db = db;
        // range.e.twi = nwr.e;
        // range.e.twi_end = nwr.e;
        // range.e.cursor = 0;
        // return range;
    }

    std::vector<WireId> getWires() const
    {
        // WireRange range;
        // range.b.chip = chip_info;
        // range.b.db = db;
        // range.b.cursor_tile = 0;
        // range.b.cursor_index = -1;
        // ++range.b; //-1 and then ++ deals with the case of no wires in the first tile
        // range.e.chip = chip_info;
        // range.e.db = db;
        // range.e.cursor_tile = chip_info->num_tiles;
        // range.e.cursor_index = 0;
        // return range;
    }

    // -------------------------------------------------

    PipId getPipByName(IdString name) const;
    IdString getPipName(PipId pip) const;

    void bindPip(PipId pip, NetInfo *net, PlaceStrength strength)
    {
        NPNR_ASSERT(pip != PipId());
        NPNR_ASSERT(pip_to_net[pip] == nullptr);

        WireId dst = canonical_wire(pip.tile, pip_data(pip).to_wire);
        NPNR_ASSERT(wire_to_net[dst] == nullptr || wire_to_net[dst] == net);

        pip_to_net[pip] = net;

        wire_to_net[dst] = net;
        net->wires[dst].pip = pip;
        net->wires[dst].strength = strength;
        refreshUiPip(pip);
        refreshUiWire(dst);
    }

    void unbindPip(PipId pip)
    {
        NPNR_ASSERT(pip != PipId());
        NPNR_ASSERT(pip_to_net[pip] != nullptr);

        WireId dst = canonical_wire(pip.tile, pip_data(pip).to_wire);
        NPNR_ASSERT(wire_to_net[dst] != nullptr);
        wire_to_net[dst] = nullptr;
        pip_to_net[pip]->wires.erase(dst);

        pip_to_net[pip] = nullptr;
        refreshUiPip(pip);
        refreshUiWire(dst);
    }

    bool checkPipAvail(PipId pip) const
    {
        NPNR_ASSERT(pip != PipId());
        return pip_to_net.find(pip) == pip_to_net.end() || pip_to_net.at(pip) == nullptr;
    }

    NetInfo *getBoundPipNet(PipId pip) const
    {
        NPNR_ASSERT(pip != PipId());
        auto p2n = pip_to_net.find(pip);
        return p2n == pip_to_net.end() ? nullptr : p2n->second;
    }

    WireId getConflictingPipWire(PipId pip) const { return getPipDstWire(pip); }

    NetInfo *getConflictingPipNet(PipId pip) const
    {
        NPNR_ASSERT(pip != PipId());
        auto p2n = pip_to_net.find(pip);
        return p2n == pip_to_net.end() ? nullptr : p2n->second;
    }

    std::vector<PipId> getPips() const
    {
        // AllPipRange range;
        // range.b.cursor_tile = 0;
        // range.b.cursor_index = -1;
        // range.b.chip = chip_info;
        // range.b.db = db;
        // ++range.b; //-1 and then ++ deals with the case of no pips in the first tile
        // range.e.cursor_tile = chip_info->width * chip_info->height;
        // range.e.cursor_index = 0;
        // range.e.chip = chip_info;
        // range.e.db = db;
        // return range;
    }

    Loc getPipLocation(PipId pip) const
    {
        Loc loc;
        loc.x = pip.tile % chip_info->width;
        loc.y = pip.tile / chip_info->width;
        loc.z = 0;
        return loc;
    }

    IdString getPipType(PipId pip) const;
    std::vector<std::pair<IdString, std::string>> getPipAttrs(PipId pip) const;

    uint32_t getPipChecksum(PipId pip) const { return pip.tile << 16 | pip.index; }

    WireId getPipSrcWire(PipId pip) const { return canonical_wire(pip.tile, pip_data(pip).from_wire); }

    WireId getPipDstWire(PipId pip) const { return canonical_wire(pip.tile, pip_data(pip).to_wire); }

    DelayInfo getPipDelay(PipId pip) const { return getDelayFromNS(0.1 + (pip.index % 30) / 1000.0); }

    std::vector<PipId> getPipsDownhill(WireId wire) const
    {
        // UpDownhillPipRange range;
        // NPNR_ASSERT(wire != WireId());
        // NeighWireRange nwr = neigh_wire_range(wire);
        // range.b.chip = chip_info;
        // range.b.db = db;
        // range.b.twi = nwr.b;
        // range.b.twi_end = nwr.e;
        // range.b.cursor = -1;
        // range.b.uphill = false;
        // ++range.b;
        // range.e.chip = chip_info;
        // range.e.db = db;
        // range.e.twi = nwr.e;
        // range.e.twi_end = nwr.e;
        // range.e.cursor = 0;
        // range.e.uphill = false;
        // return range;
    }

    std::vector<PipId> getPipsUphill(WireId wire) const
    {
        // UpDownhillPipRange range;
        // NPNR_ASSERT(wire != WireId());
        // NeighWireRange nwr = neigh_wire_range(wire);
        // range.b.chip = chip_info;
        // range.b.db = db;
        // range.b.twi = nwr.b;
        // range.b.twi_end = nwr.e;
        // range.b.cursor = -1;
        // range.b.uphill = true;
        // ++range.b;
        // range.e.chip = chip_info;
        // range.e.db = db;
        // range.e.twi = nwr.e;
        // range.e.twi_end = nwr.e;
        // range.e.cursor = 0;
        // range.e.uphill = true;
        // return range;
    }

    std::vector<PipId> getWireAliases(WireId wire) const
    {
        // UpDownhillPipRange range;
        // range.b.cursor = 0;
        // range.b.twi.cursor = 0;
        // range.e.cursor = 0;
        // range.e.twi.cursor = 0;
        // return range;
    }

    // -------------------------------------------------

    GroupId getGroupByName(IdString name) const { return GroupId(); }
    IdString getGroupName(GroupId group) const { return IdString(); }
    std::vector<GroupId> getGroups() const { return {}; }
    std::vector<BelId> getGroupBels(GroupId group) const { return {}; }
    std::vector<WireId> getGroupWires(GroupId group) const { return {}; }
    std::vector<PipId> getGroupPips(GroupId group) const { return {}; }
    std::vector<GroupId> getGroupGroups(GroupId group) const { return {}; }

    // -------------------------------------------------

    delay_t estimateDelay(WireId src, WireId dst) const;
    delay_t predictDelay(const NetInfo *net_info, const PortRef &sink) const;
    delay_t getDelayEpsilon() const { return 20; }
    delay_t getRipupDelayPenalty() const { return 120; }
    delay_t getWireRipupDelayPenalty(WireId wire) const;
    float getDelayNS(delay_t v) const { return v; }
    DelayInfo getDelayFromNS(float ns) const
    {
        DelayInfo del;
        del.min_raise_delay = ns;
        del.max_raise_delay = ns;
        del.min_fall_delay = ns;
        del.max_fall_delay = ns;
        return del;
    }
    uint32_t getDelayChecksum(delay_t v) const { return * ( uint32_t * ) &v; }
    bool getBudgetOverride(const NetInfo *net_info, const PortRef &sink, delay_t &budget) const;
    ArcBounds getRouteBoundingBox(WireId src, WireId dst) const;

    // -------------------------------------------------

    // Get the delay through a cell from one port to another, returning false
    // if no path exists. This only considers combinational delays, as required by the Arch API
    bool getCellDelay(const CellInfo *cell, IdString fromPort, IdString toPort, DelayInfo &delay) const;
    // Get the port class, also setting clockInfoCount to the number of TimingClockingInfos associated with a port
    TimingPortClass getPortTimingClass(const CellInfo *cell, IdString port, int &clockInfoCount) const;
    // Get the TimingClockingInfo of a port
    TimingClockingInfo getPortClockingInfo(const CellInfo *cell, IdString port, int index) const;

    // -------------------------------------------------

    // Perform placement validity checks, returning false on failure (all
    // implemented in arch_place.cc)

    // Whether or not a given cell can be placed at a given Bel
    // This is not intended for Bel type checks, but finer-grained constraints
    // such as conflicting set/reset signals, etc
    bool isValidBelForCell(CellInfo *cell, BelId bel) const;

    // Return true whether all Bels at a given location are valid
    bool isBelLocationValid(BelId bel) const;

    // -------------------------------------------------

    bool pack();
    bool place();
    bool route();

    // -------------------------------------------------
    // Assign architecure-specific arguments to nets and cells, which must be
    // called between packing or further
    // netlist modifications, and validity checks
    void assignArchInfo();
    void assignCellInfo(CellInfo *cell);

    // -------------------------------------------------
    // Arch-specific global routing
    void route_globals();

    // -------------------------------------------------

    std::vector<GraphicElement> getDecalGraphics(DecalId decal) const;

    DecalXY getBelDecal(BelId bel) const;
    DecalXY getWireDecal(WireId wire) const;
    DecalXY getPipDecal(PipId pip) const;
    DecalXY getGroupDecal(GroupId group) const;

    // -------------------------------------------------

    static const std::string defaultPlacer;
    static const std::vector<std::string> availablePlacers;
    static const std::string defaultRouter;
    static const std::vector<std::string> availableRouters;

    // -------------------------------------------------

    template <typename Id> const LocTypePOD &loc_data(const Id &id) const { return chip_loc_data(db, chip_info, id); }

    // template <typename Id> const LocNeighourhoodPOD &nh_data(const Id &id) const
    // {
    //     return chip_nh_data(db, chip_info, id);
    // }

    inline const BelInfoPOD &bel_data(BelId id) const { return chip_bel_data(db, chip_info, id); }
    inline const LocWireInfoPOD &wire_data(WireId id) const { return chip_wire_data(db, chip_info, id); }
    inline const PipInfoPOD &pip_data(PipId id) const { return chip_pip_data(db, chip_info, id); }
    inline bool rel_tile(int32_t base, int16_t rel_x, int16_t rel_y, int32_t &next) const
    {
        return chip_rel_tile(chip_info, base, rel_x, rel_y, next);
    }
    inline WireId canonical_wire(int32_t tile, uint16_t index) const
    {
        WireId c = chip_canonical_wire(db, chip_info, tile, index);
        return c;
    }
    IdString pip_src_wire_name(PipId pip) const
    {
        // int wire = pip_data(pip).from_wire;
        // return db->loctypes[chip_info->grid[pip.tile].loc_type].wires[wire].name;
    }
    IdString pip_dst_wire_name(PipId pip) const
    {
        // int wire = pip_data(pip).to_wire;
        // return db->loctypes[chip_info->grid[pip.tile].loc_type].wires[wire].name;
    }

    // // -------------------------------------------------

    // typedef std::unordered_map<IdString, CellPinStyle> CellPinsData;

    // std::unordered_map<IdString, CellPinsData> cell_pins_db;
    // CellPinStyle get_cell_pin_style(const CellInfo *cell, IdString port) const;

    // void init_cell_pin_data();

    // // -------------------------------------------------

    // // Parse a possibly-Lattice-style (C literal in Verilog string) style parameter
    // Property parse_lattice_param(const CellInfo *ci, IdString prop, int width, int64_t defval) const;

    // // -------------------------------------------------

    // NeighWireRange neigh_wire_range(WireId wire) const
    // {
    //     NeighWireRange range;
    //     range.b.chip = chip_info;
    //     range.b.db = db;
    //     range.b.baseWire = wire;
    //     range.b.cursor = -1;

    //     range.e.chip = chip_info;
    //     range.e.db = db;
    //     range.e.baseWire = wire;
    //     range.e.cursor = nh_data(wire).wire_neighbours[wire.index].num_nwires;
    //     return range;
    // }

    // // -------------------------------------------------

    // template <typename TId> uint32_t tile_loc_flags(TId id) const { return chip_info->grid[id.tile].loc_flags; }

    // template <typename TId> bool tile_is(TId id, LocFlags lf) const { return tile_loc_flags(id) & lf; }

    // // -------------------------------------------------

    // enum LogicBelZ
    // {
    //     BEL_LUT0 = 0,
    //     BEL_LUT1 = 1,
    //     BEL_FF0 = 2,
    //     BEL_FF1 = 3,
    //     BEL_RAMW = 4,
    // };

    // void update_logic_bel(BelId bel, CellInfo *cell)
    // {
    //     int z = bel_data(bel).z;
    //     NPNR_ASSERT(z < 32);
    //     auto &tts = tileStatus[bel.tile];
    //     if (tts.lts == nullptr)
    //         tts.lts = new LogicTileStatus();
    //     auto &ts = *(tts.lts);
    //     ts.cells[z] = cell;
    //     switch (z & 0x7) {
    //     case BEL_FF0:
    //     case BEL_FF1:
    //     case BEL_RAMW:
    //         ts.halfs[(z >> 3) / 2].dirty = true;
    //     /* fall-through */
    //     case BEL_LUT0:
    //     case BEL_LUT1:
    //         ts.slices[(z >> 3)].dirty = true;
    //         break;
    //     }
    // }

    // bool nexus_logic_tile_valid(LogicTileStatus &lts) const;

    // CellPinMux get_cell_pinmux(const CellInfo *cell, IdString pin) const;
    // void set_cell_pinmux(CellInfo *cell, IdString pin, CellPinMux state);

    // // -------------------------------------------------

    // const PadInfoPOD *get_pkg_pin_data(const std::string &pin) const;
    // Loc get_pad_loc(const PadInfoPOD *pad) const;
    // BelId get_pad_pio_bel(const PadInfoPOD *pad) const;
    // const PadInfoPOD *get_bel_pad(BelId bel) const;
    // std::string get_pad_functions(const PadInfoPOD *pad) const;

    // // -------------------------------------------------
    // // Data about different IO standard, mostly used by bitgen
    // static const std::unordered_map<std::string, IOTypeData> io_types;
    // int get_io_type_vcc(const std::string &io_type) const;
    // bool is_io_type_diff(const std::string &io_type) const;
    // bool is_io_type_ref(const std::string &io_type) const;

    // // -------------------------------------------------

    // // List of IO constraints, used by PDC parser
    // std::unordered_map<IdString, std::unordered_map<IdString, Property>> io_attr;

    // void read_pdc(std::istream &in);

    // // -------------------------------------------------
    // void write_fasm(std::ostream &out) const;
};

NEXTPNR_NAMESPACE_END
