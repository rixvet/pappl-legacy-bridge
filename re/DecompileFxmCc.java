// Decompile the FXM_CC colour-conversion functions to re/FXM_CC-decompiled.c
// Run: support/analyzeHeadless <projdir> fxmcc -import analysis/tarball/usr/lib/cups/filter/FXM_CC \
//        -postScript DecompileFxmCc.java -scriptPath re -deleteProject
// @category RE
import ghidra.app.script.GhidraScript;
import ghidra.app.decompiler.DecompInterface;
import ghidra.app.decompiler.DecompileResults;
import ghidra.program.model.listing.Function;
import java.io.FileWriter;
import java.io.PrintWriter;

public class DecompileFxmCc extends GhidraScript {
    @Override
    public void run() throws Exception {
        String[] funcs = {
            "main",
            "cil_Create_LCC_Param_sBGR",
            "cil_Create_CST_Param_WpGm",
            "cil_LCC_Conv_sBGR",
            "cil_LCC_Conv_sBGR_Pixel",
            "cil_CST_Conv_sBGR",
            "cil_CST_Conv_sBGR_Pixel",
            "cil_NTSC_Conv",
            "cil_NTSC_Conv_Pixel",
            "CheckIfWhiteData",
            "ReadDTFInfo",
            "GetDeviceColorParam",
            "GetColorSetting",
            "cil_3Dlut_Conv_sBGR",
            "cil_3Dlut_Conv_sBGR_Pixel",
            "cil_Create_3Dlut_Param_RGB"
        };
        PrintWriter out = new PrintWriter(new FileWriter(
                "/home/rick/src/phaser6125/re/FXM_CC-decompiled.c"));
        out.println("/* FXM_CC (Xerox/Fuji 32-bit CUPS colour-correction filter) - Ghidra decompilation.");
        out.println(" * Binary: analysis/tarball/usr/lib/cups/filter/FXM_CC (ELF32 i386, with symbols).");
        out.println(" * Regenerate with re/DecompileFxmCc.java via analyzeHeadless (see RE-NOTES.md).");
        out.println(" * Interoperability research only; do not redistribute the original binary. */");
        out.println();
        DecompInterface di = new DecompInterface();
        di.openProgram(currentProgram);
        for (String name : funcs) {
            Function f = null;
            for (Function fn : currentProgram.getFunctionManager().getFunctions(true)) {
                if (fn.getName().equals(name)) { f = fn; break; }
            }
            out.println("//// ================= " + name + " @ " +
                    (f == null ? "?" : f.getEntryPoint().toString()) + " =================");
            if (f == null) {
                out.println("/* NOT FOUND */");
                out.println();
                continue;
            }
            DecompileResults res = di.decompileFunction(f, 180, monitor);
            if (res.decompileCompleted()) {
                out.println(res.getDecompiledFunction().getC());
            } else {
                out.println("/* decompile failed: " + res.getErrorMessage() + " */");
            }
            out.println();
        }
        out.close();
        println("WROTE re/FXM_CC-decompiled.c");
    }
}
