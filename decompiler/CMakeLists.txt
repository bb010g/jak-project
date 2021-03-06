add_library(
        decomp
        SHARED

        analysis/atomic_op_builder.cpp
        analysis/cfg_builder.cpp
        analysis/expression_build.cpp
        analysis/final_output.cpp
        analysis/reg_usage.cpp
        analysis/variable_naming.cpp

        data/game_count.cpp
        data/game_text.cpp
        data/StrFileReader.cpp
        data/tpage.cpp

        Disasm/Instruction.cpp
        Disasm/InstructionDecode.cpp
        Disasm/InstructionMatching.cpp
        Disasm/InstructionParser.cpp
        Disasm/OpcodeInfo.cpp
        Disasm/Register.cpp

        Function/BasicBlocks.cpp
        Function/CfgVtx.cpp
        Function/Function.cpp
        Function/TypeAnalysis.cpp
        Function/TypeInspector.cpp

        IR/BasicOpBuilder.cpp
        IR/IR.cpp

        IR2/AtomicOp.cpp
        IR2/AtomicOpForm.cpp
        IR2/AtomicOpTypeAnalysis.cpp
        IR2/Env.cpp
        IR2/Form.cpp
        IR2/FormExpressionAnalysis.cpp
        IR2/FormStack.cpp
        IR2/GenericElementMatcher.cpp

        ObjectFile/LinkedObjectFile.cpp
        ObjectFile/LinkedObjectFileCreation.cpp
        ObjectFile/ObjectFileDB.cpp
        ObjectFile/ObjectFileDB_IR2.cpp

        util/DecompilerTypeSystem.cpp
        util/TP_Type.cpp

        config.cpp
)

target_link_libraries(decomp
        lzokay
        common
        fmt
        )

add_executable(decompiler
        main.cpp
        )

target_link_libraries(decompiler
        decomp
        common
        lzokay
        fmt)

install(TARGETS decompiler)
