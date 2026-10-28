import re, subprocess
txt = open('FXM_ALC_c525.dwarf').read()
blocks = re.split(r'(?=<[0-9]+><[0-9a-f]+>: Abbrev Number:)', txt)
raw = subprocess.run(['objdump','--dwarf=rawline','/home/rick/src/phaser6125/analysis/tarball/usr/lib/cups/filter/FXM_ALC'],capture_output=True,text=True).stdout
ftabs = {}
for m in re.finditer(r'The File Name Table [(]offset (0x[0-9a-f]+)[)]:\n((?:[ ]+[0-9]+\t.*\n)+)', raw):
    off = int(m.group(1),16); tab = {}
    for line in m.group(2).strip().split('\n'):
        p = line.split('\t')
        if len(p)>=5 and p[0].strip().isdigit(): tab[int(p[0].strip())] = p[4].strip()
    ftabs[off] = tab
cur = None
out = []
NAME = re.compile(r'DW_AT_name[ ]+:[ ]+(?:[(]indirect string, offset: 0x[0-9a-f]+[)]:[ ]*)?(\S+)')
for b in blocks:
    m = re.match(r'<([0-9]+)><([0-9a-f]+)>: Abbrev Number: ([0-9]+) .DW_TAG_([a-z_]+).', b)
    if not m: continue
    depth, tag = int(m.group(1)), 'DW_TAG_' + m.group(4)
    if depth==0 and tag=='DW_TAG_compile_unit':
        nm = NAME.search(b)
        sl = re.search(r'DW_AT_stmt_list[ ]+:[ ]+(0x[0-9a-f]+|[0-9]+)', b)
        tab = ftabs.get(int(sl.group(1),0), {}) if sl else {}
        cur = (nm.group(1) if nm else '?', tab)
        out.append('\n##### CU: ' + cur[0])
        continue
    if depth==1 and tag=='DW_TAG_subprogram' and cur:
        nm = NAME.search(b)
        if not nm: continue
        low = re.search(r'DW_AT_low_pc[ ]+:[ ]+(0x[0-9a-f]+)', b)
        high = re.search(r'DW_AT_high_pc[ ]+:[ ]+(0x[0-9a-f]+)', b)
        decl = re.search(r'DW_AT_decl_file[ ]+:[ ]+([0-9]+)', b)
        line = re.search(r'DW_AT_decl_line[ ]+:[ ]+([0-9]+)', b)
        if not low: continue
        fn = cur[1].get(int(decl.group(1)), 'file#'+decl.group(1)) if decl else '?'
        sz = (int(high.group(1),16)-int(low.group(1),16)) if high else 0
        out.append('  %s %5d %-42s %s:%s' % (low.group(1), sz, nm.group(1), fn, line.group(1) if line else '?'))
open('subprograms.txt','w').write('\n'.join(out))
print('wrote', len(out), 'lines, blocks:', len(blocks))
