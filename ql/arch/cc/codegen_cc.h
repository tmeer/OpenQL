/**
 * @file   codegen_cc.h
 * @date   201810xx
 * @author Wouter Vlothuizen (wouter.vlothuizen@tno.nl)
 * @brief  code generator backend for the Central Controller
 */

// FIXME: this should be the only backend specific code
// FIXME: check call structure of if_start here? Better in caller?
// FIXME: manage stringstream here


#ifndef QL_ARCH_CC_CODEGEN_CC_H
#define QL_ARCH_CC_CODEGEN_CC_H

namespace ql
{
namespace arch
{


// from: https://stackoverflow.com/questions/5878775/how-to-find-and-replace-string
template <typename T, typename U>
T &replace (
          T &str,
    const U &from,
    const U &to)
{
    size_t pos;
    size_t offset = 0;
    const size_t increment = to.size();

    while ((pos = str.find(from, offset)) != T::npos)
    {
        str.replace(pos, from.size(), to);
        offset = pos + increment;
    }

    return str;
}



class codegen_cc
{
public:

    codegen_cc()
    {
    }


    ~codegen_cc()
    {
    }

    /************************************************************************\
    | Generic
    \************************************************************************/

    void init(const ql::quantum_platform& platform)
    {
        load_backend_settings(platform);
    }

    std::string getCode()
    {
        return cccode.str();
    }

    void program_start(std::string prog_name)
    {
        // FIXME: clear codewordTable

        // emit program header
        cccode << std::left;                                // assumed by emit()
        cccode << "# Program: '" << prog_name << "'" << std::endl;
        cccode << "# Note:    generated by OpenQL Central Controller backend" << std::endl;
        cccode << "#" << std::endl;
    }

    void program_finish()
    {
        emit("", "stop");                                  // NB: cc_light loops whole program indefinitely

        std::cout << std::setw(4) << codewordTable << std::endl; // FIXME
    }

    void bundle_start(int delta, std::string cmnt)   // FIXME: do we need parameter delta
    {
        // empty the matrix of signal values
        size_t slotsUsed = ccSetup["slots"].size();
        size_t maxGroups = 32;                     // FIXME: magic constant, enough for VSM
        signalValues.assign(slotsUsed, std::vector<string>(maxGroups, ""));

        comment(cmnt);
    }

    void bundle_finish(int duration_in_cycles, int delta)   // FIXME: do we need parameter delta
    {
        const json &ccSetupSlots = ccSetup["slots"];
        for(size_t slotIdx=0; slotIdx<signalValues.size(); slotIdx++) {         // iterate over slot vector
            // collect info from JSON
            const json &ccSetupSlot = ccSetupSlots[slotIdx];
            const json &instrument = ccSetupSlot["instrument"];
            std::string instrumentName = instrument["name"];
            int slot = ccSetupSlot["slot"];

            bool isSlotUsed = false;
            uint32_t digOut = 0;
            size_t numGroups = signalValues[slotIdx].size();
            for(size_t group=0; group<numGroups; group++) {                     // iterate over groups used within slot
                if(signalValues[slotIdx][group] != "") {
                    isSlotUsed = true;

                    // find control mode & bits
                    std::string controlModeName = instrument["control_mode"];
                    const json &controlMode = controlModes[controlModeName];    // the control mode definition for our instrument
                    const json &myControlBits = controlMode["control_bits"][group];

                    // find or create codeword/mask fragment for this group
                    DOUT("instrumentName=" << instrumentName <<
                         ", slot=" << slot <<
                         ", group=" << group <<
                        ", control bits: " << myControlBits);
                    size_t numBits = myControlBits.size();
                    if(numBits == 1) {      // single bit, implying this is a mask (not code word)
                        digOut |= 1<<(int)myControlBits[0];     // NB: we assume the mask is active high, which is correct for VSM and UHF-QC
                    } else {                // > 1 bit, implying code word
                        // FIXME allow single code word for vector of groups
                        // try to find code word
                        uint32_t codeWord = 0;
                        std::string signalValue = signalValues[slotIdx][group];

                        if(JSON_EXISTS(codewordTable, instrumentName) &&                    // instrument exists
                                        codewordTable[instrumentName].size() > group) {     // group exists
                            bool cwFound = false;
                           // try to find signalValue
                            for(codeWord=0; codeWord<codewordTable[instrumentName][group].size() && !cwFound; codeWord++) {   // NB: JSON find() doesn't work for arrays
                                if(codewordTable[instrumentName][group][codeWord] == signalValue) {
                                    DOUT("signal value found at cw=" << codeWord);
                                    cwFound = true;
                                }
                            }
                            if(!cwFound) {
                                DOUT("signal value '" << signalValue << "' not found in group " << group << ", which contains " << codewordTable[instrumentName][group]);
                                // NB: codeWord already contains last used value + 1
                                // FIXME: check that number is available
                                codewordTable[instrumentName][group][codeWord] = signalValue;   // NB: structure created on demand
                            }

//                            std::cout << std::setw(4) << codewordTable << std::endl; // FIXME
                        } else {    // new instrument/group
                            codeWord = 1;
                            codewordTable[instrumentName][group][0] = "";               // code word 0 is empty
                            codewordTable[instrumentName][group][codeWord] = signalValue;    // NB: structure created on demand
//                            std::cout << std::setw(4) << codewordTable << std::endl; // FIXME
                        }

                        // convert codeWord to digOut
                        for(size_t idx=0; idx<numBits; idx++) {
                            int codeWordBit = numBits-1-idx;    // myControlBits defines MSB..LSB
                            if(codeWord & (1<<codeWordBit)) digOut |= 1<<(int)myControlBits[idx];
                        }
                    }

                    // add trigger to digOut
                    size_t triggersSize = controlMode["triggers"].size();
                    if(triggersSize == 0) {         // no trigger
                        // do nothing
                    } else if(triggersSize == 1) {  // single trigger for all groups
                        digOut |= 1 << (int)controlMode["triggers"][0];
                    } else {                        // trigger per group
                        digOut |= 1 << (int)controlMode["triggers"][group];
                        // FIXME: check validity of triggersSize
                    }
                }
            }

            if(isSlotUsed) {
                // emit code for slot
                emit("", "seq_out",
                     SS2S(slot <<
                          ",0x" << std::hex << std::setw(8) << std::setfill('0') << digOut <<
                          "," << duration_in_cycles),
                          std::string("# code word/mask on '"+instrumentName+"'").c_str());
                    // FIXME: for codewords there is no problem if duration>gate time, but for VSM there is!
            } else {
                // slot not used for this gate, generate delay
                emit("", "seq_out", SS2S(slot << ",0x00000000," << duration_in_cycles), std::string("# idle on '"+instrumentName+"'").c_str());
            }
        }

        comment("");    // blank line to separate bundles
    }

    void comment(std::string c)
    {
        if(verboseCode) emit(c.c_str());
    }

    /************************************************************************\
    | Quantum instructions
    \************************************************************************/

    void nop_gate()
    {
        comment("# NOP gate");
        FATAL("FIXME: not implemented");
    }

    // single/two/N qubit gate
    // FIXME: remove parameter platform
    void custom_gate(std::string iname, std::vector<size_t> ops, const ql::quantum_platform& platform)
    {
        // generate comment
        std::stringstream cmnt;
        cmnt << " # gate '" << iname << " ";
        for(size_t i=0; i<ops.size(); i++) {
            cmnt << ops[i];
            if(i<ops.size()-1) cmnt << ",";
        }
        cmnt << "'";
        comment(cmnt.str());

        // find signal definition for iname
        const json &instruction = platform.find_instruction(iname);
        const json *tmp;
        if(JSON_EXISTS(instruction["cc"], "signal_ref")) {
            std::string signalRef = instruction["cc"]["signal_ref"];
            tmp = &signals[signalRef];  // poor man's JSON pointer
            if(tmp->size() == 0) {
                FATAL("Error in JSON definition of instruction '" << iname <<
                      "': signal_ref '" << signalRef << "' does not resolve");
            }
        } else {
            tmp = &instruction["cc"]["signal"];
            DOUT("signal for '" << instruction << "': " << *tmp);
        }
        const json &signal = *tmp;

        // iterate over signals defined in instruction
        for(size_t s=0; s<signal.size(); s++) {
            // get the qubit to work on
            size_t operandIdx = signal[s]["operand_idx"];
            if(operandIdx >= ops.size()) {
                FATAL("Error in JSON definition of instruction '" << iname <<
                      "': illegal operand number " << operandIdx <<
                      "' exceeds expected maximum of " << ops.size())
            }
            size_t qubit = ops[operandIdx];

            // FIXME: cross check that gate duration fits within bundle?

            // get the instrument and group that generates the signal
            std::string instructionSignalType = signal[s]["type"];
            const json &instructionSignalValue = signal[s]["value"];
            tSignalInfo si = findSignalInfoForQubit(instructionSignalType, qubit);
            const json &ccSetupSlot = ccSetup["slots"][si.slotIdx];
            std::string instrumentName = ccSetupSlot["instrument"]["name"];
            int slot = ccSetupSlot["slot"];

            // expand macros in signalValue
            std::string signalValueString = SS2S(instructionSignalValue);   // serialize instructionSignalValue into std::string
            replace(signalValueString, std::string("{gateName}"), iname);
            replace(signalValueString, std::string("{instrumentName}"), instrumentName);
            replace(signalValueString, std::string("{instrumentGroup}"), std::to_string(si.group));
            replace(signalValueString, std::string("{qubit}"), std::to_string(qubit));

            comment(SS2S("  # slot=" << slot <<
                         ", group=" << si.group <<
                         ", instrument='" << instrumentName <<
                         "', signal='" << signalValueString << "'"));

            // check and store signal value
            // make room in signalValues matrix
// FIXME            signalValues.reserve(si.slotIdx+1);
//            signalValues[si.slotIdx].reserve(si.group+1);
            if(signalValues[si.slotIdx][si.group] == "") {                          // not yet used
                signalValues[si.slotIdx][si.group] = signalValueString;
            } else if(signalValues[si.slotIdx][si.group] == signalValueString) {    // unchanged
                // do nothing
            } else {
                EOUT("Code so far:\n" << cccode.str());                    // FIXME: provide context to help finding reason
                FATAL("Signal conflict on instrument='" << instrumentName <<
                      "', group=" << si.group <<
                      ", between '" << signalValues[si.slotIdx][si.group] <<
                      "' and '" << signalValueString << "'");
            }

            // NB: code is generated in bundle_finish()
        }
    }

    /************************************************************************\
    | Readout
    \************************************************************************/

    void readout(size_t cop, size_t qop)
    {
        comment(SS2S("# READOUT(c" << cop << ",q" << qop << ")"));
//        FATAL("FIXME: not implemented");
    }

    /************************************************************************\
    | Classical operations on kernels
    \************************************************************************/

    void if_start(size_t op0, std::string opName, size_t op1)
    {
        comment(SS2S("# IF_START(R" << op0 << " " << opName << " R" << op1 << ")"));
        FATAL("FIXME: not implemented");
    }

    void else_start(size_t op0, std::string opName, size_t op1)
    {
        comment(SS2S("# ELSE_START(R" << op0 << " " << opName << " R" << op1 << ")"));
        FATAL("FIXME: not implemented");
    }

    void for_start(std::string label, int iterations)
    {
        comment(SS2S("# FOR_START(" << iterations << ")"));
        // FIXME: reserve register
        emit((label+":").c_str(), "move", SS2S(iterations << ",R63"), "# R63 is the 'for loop counter'");        // FIXME: fixed reg, no nested loops
    }

    void for_end(std::string label)
    {
        comment("# FOR_END");
        // FIXME: free register
        emit("", "loop", SS2S("R63,@" << label), "# R63 is the 'for loop counter'");        // FIXME: fixed reg, no nested loops
    }

    void do_while_start(std::string label)
    {
        comment("# DO_WHILE_START");
//        FATAL("FIXME: not implemented");    // FIXME: implement: emit label
    }

    void do_while_end(size_t op0, std::string opName, size_t op1)
    {
        comment(SS2S("# DO_WHILE_END(R" << op0 << " " << opName << " R" << op1 << ")"));
//        FATAL("FIXME: not implemented");
    }

    /************************************************************************\
    | Classical arithmetic instructions
    \************************************************************************/

    void add() {
        FATAL("FIXME: not implemented");
    }

    // FIXME: etc


private:
    typedef struct {
        int slotIdx;    // index into cc_setup["slots"]
        int group;
    } tSignalInfo;


private:
    bool verboseCode = true;                                    // output extra comments in generated code

    std::stringstream cccode;                                   // the code generated for the CC
    std::vector<std::vector<std::string>> signalValues;         // matrix[slotIdx][group]
    json codewordTable;

    // some JSON nodes we need access to. FIXME: use pointers for efficiency?
    json backendSettings;
    json instrumentDefinitions;
    json controlModes;
    json ccSetup;
    json signals;

    /************************************************************************\
    | Some helpers to ease nice assembly formatting
    \************************************************************************/

    void emit(const char *labelOrComment, const char *instr="")
    {
        if(!labelOrComment || strlen(labelOrComment)==0) {  // no label
            cccode << "        " << instr << std::endl;
        } else if(strlen(labelOrComment)<8) {               // label fits before instr
            cccode << std::setw(8) << labelOrComment << instr << std::endl;
        } else if(strlen(instr)==0) {                       // no instr
            cccode << labelOrComment << std::endl;
        } else {
            cccode << labelOrComment << std::endl << "        " << instr << std::endl;
        }
    }

    // @param   label       must include trailing ":"
    // @param   comment     must include leading "#"
    void emit(const char *label, const char *instr, std::string ops, const char *comment="")
    {
        cccode << std::setw(8) << label << std::setw(8) << instr << std::setw(24) << ops << comment << std::endl;
    }

    // FIXME: also provide these with std::string parameters

    /************************************************************************\
    | Functions processing JSON
    \************************************************************************/

    void load_backend_settings(const ql::quantum_platform& platform)
    {
        // parts of JSON syntax
        const char *instrumentTypes[] = {"cc", "switch", "awg", "measure"};

        // remind some main JSON areas
        backendSettings = platform.hardware_settings["eqasm_backend_cc"];
        instrumentDefinitions = backendSettings["instrument_definitions"];
        controlModes = backendSettings["control_modes"];
        ccSetup = backendSettings["cc_setup"];
        signals = backendSettings["signals"];


        // read instrument definitions
        for(size_t i=0; i<ELEM_CNT(instrumentTypes); i++)
        {
            const json &ids = instrumentDefinitions[instrumentTypes[i]];
            // FIXME: the following requires json>v3.1.0:  for(auto& id : ids.items()) {
            for(size_t j=0; j<ids.size(); j++) {
                std::string idName = ids[j]["name"];        // NB: uses type conversion to get node value
                DOUT("found instrument definition:  type='" << instrumentTypes[i] << "', name='" << idName <<"'");
            }
        }

        // read control modes
#if 0   // FIXME
        for(size_t i=0; i<controlModes.size(); i++)
        {
            const json &name = controlModes[i]["name"];
            DOUT("found control mode '" << name <<"'");
        }
#endif

        // read instruments
        // const json &ccSetupType = ccSetup["type"];

        // CC specific
        const json &ccSetupSlots = ccSetup["slots"];                      // FIXME: check against instrumentDefinitions
        for(size_t slot=0; slot<ccSetupSlots.size(); slot++) {
            const json &instrument = ccSetupSlots[slot]["instrument"];
            std::string instrumentName = instrument["name"];
            std::string signalType = instrument["signal_type"];

            DOUT("found instrument: name='" << instrumentName << "signal type='" << signalType << "'");
        }
    }


    // find instrument/group/slot providing instructionSignalType for qubit
    tSignalInfo findSignalInfoForQubit(std::string instructionSignalType, size_t qubit)
    {
        tSignalInfo ret = {-1, -1};
        bool signalTypeFound = false;
        bool qubitFound = false;

        // iterate over CC slots
        const json &ccSetupSlots = ccSetup["slots"];
        for(size_t slotIdx=0; slotIdx<ccSetupSlots.size(); slotIdx++) {
            const json &ccSetupSlot = ccSetupSlots[slotIdx];
            const json &instrument = ccSetupSlot["instrument"];
            std::string instrumentSignalType = instrument["signal_type"];
            if(instrumentSignalType == instructionSignalType) {
                signalTypeFound = true;
                std::string instrumentName = instrument["name"];
                const json &qubits = instrument["qubits"];
                // FIXME: verify group size
                // FIXME: verify signal dimensions

                // anyone connected to qubit?
                for(size_t group=0; group<qubits.size() && !qubitFound; group++) {
                    for(size_t idx=0; idx<qubits[group].size() && !qubitFound; idx++) {
                        if(qubits[group][idx] == qubit) {
                            qubitFound = true;

                            DOUT("qubit " << qubit <<
                                 " signal type '" << instructionSignalType <<
                                 "' driven by instrument '" << instrumentName <<
                                 "' group " << group <<
                                 " in CC slot " << ccSetupSlot["slot"]);

                            ret.slotIdx = slotIdx;
                            ret.group = group;
                        }
                    }
                }
            }
        }
        if(!signalTypeFound) {
            FATAL("No instruments found providing signal type '" << instructionSignalType << "'");     // FIXME: clarify for user
        }
        if(!qubitFound) {
            FATAL("No instruments found driving qubit " << qubit << " for signal type '" << instructionSignalType << "'");     // FIXME: clarify for user
        }

        return ret;
    }

}; // class

} // arch
} // ql


#if 0   // FIXME: old code that may me useful
    // information extracted from JSON file:
    typedef std::string tSignalType;
    typedef std::vector<json> tInstrumentList;
    typedef std::map<tSignalType, tInstrumentList> tMapSignalTypeToInstrumentList;
    tMapSignalTypeToInstrumentList mapSignalTypeToInstrumentList;


    // insert into map, so we can easily retrieve which instruments provide "signal_type"
    tMapSignalTypeToInstrumentList::iterator it = mapSignalTypeToInstrumentList.find(signalType);
    if(it != mapSignalTypeToInstrumentList.end()) {    // key exists
        tInstrumentList &instrumentList = it->second;
        instrumentList.push_back(instrument);
    } else { // new key
        std::pair<tMapSignalTypeToInstrumentList::iterator,bool> rslt;
        tInstrumentList instrumentList;
        instrumentList.push_back(instrument);
        rslt = mapSignalTypeToInstrumentList.insert(std::make_pair(signalType, instrumentList));
    }


    // instruments providing these signal type
    // FIXME: just walk the CC slots?
    tMapSignalTypeToInstrumentList::iterator it = mapSignalTypeToInstrumentList.find(signalType);
    if(it != mapSignalTypeToInstrumentList.end()) {    // key exists
        tInstrumentList &instrumentList = it->second;
        for(size_t i=0; i<instrumentList.size(); i++) {

#endif

#endif  // ndef QL_ARCH_CC_CODEGEN_CC_H
