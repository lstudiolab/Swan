import os;

fn is_space(c: u8) -> bool {
    c == ' ' || c == '\t' || c == '\r'
}

fn append_c_string(out: u8[>..]&, value: u8[:]) {
    out.push('"');
    for i in 0..value.len {
        let c = value[i];
        if c == '"' { out.append("\\\""); }
        else if c == '\\' { out.append("\\\\"); }
        else if c == '\n' { out.append("\\n"); }
        else if c == '\r' { out.append("\\r"); }
        else if c == '\t' { out.append("\\t"); }
        else { out.push(c); }
    }
    out.push('"');
}

// First vertical slice: transpile a deliberately small, explicit Swan subset.
// Unsupported statements fail instead of producing misleading C output.
fn transpile(source: u8[:], output: u8[>..]&) -> bool {
    output.append("#include <stdio.h>\n\nint main(void) {\n");
    var offset = 0;
    var saw_main = false;
    var closed_main = false;

    while offset < source.len {
        let line_start = offset;
        while offset < source.len && source[offset] != '\n' { offset += 1; }
        let line_end = offset;
        if offset < source.len { offset += 1; }

        var start = line_start;
        while start < line_end && is_space(source[start]) { start += 1; }
        var end = line_end;
        while end > start && is_space(source[end - 1]) { end -= 1; }
        if start == end { continue; }

        let line = source[start..end];

        if line == "fn main():" {
            if saw_main {
                print("Swan error: duplicate main function");
                return false;
            }
            saw_main = true;
            continue;
        }

        if !saw_main || closed_main {
            print("Swan error: this first compiler version supports one fn main() only");
            return false;
        }

        if line.len >= 4 && line[..4] == "pass" {
            output.append("    (void)0;\n");
            continue;
        }

        if line.len >= 3 && line[..3] == "log" {
            var q = 3;
            while q < line.len && is_space(line[q]) { q += 1; }
            if q >= line.len || (line[q] != '"' && line[q] != '\'') {
                print("Swan error: log currently requires a string literal");
                return false;
            }
            let quote = line[q];
            q += 1;
            let value_start = q;
            while q < line.len && line[q] != quote { q += 1; }
            if q >= line.len {
                print("Swan error: unterminated string in log statement");
                return false;
            }
            let value = line[value_start..q];
            q += 1;
            while q < line.len && is_space(line[q]) { q += 1; }
            if q < line.len && line[q] == ';' { q += 1; }
            if q != line.len {
                print("Swan error: unexpected text after log string");
                return false;
            }
            output.append("    puts(");
            append_c_string(output, value);
            output.append(");\n");
            continue;
        }

        if line.len >= 6 && line[..6] == "return" {
            var q = 6;
            while q < line.len && is_space(line[q]) { q += 1; }
            var value_end = line.len;
            if value_end > q && line[value_end - 1] == ';' { value_end -= 1; }
            if q == value_end {
                output.append("    return 0;\n");
            } else {
                let value = line[q..value_end];
                output.append("    return ");
                output.append(value);
                output.append(";\n");
            }
            closed_main = true;
            continue;
        }

        print("Swan error: unsupported statement in initial Goose compiler subset: ", line);
        return false;
    }

    if !saw_main {
        print("Swan error: expected fn main():");
        return false;
    }
    if !closed_main { output.append("    return 0;\n"); }
    output.append("}\n");
    return true;
}

fn main() {
    let args = os::args();
    if args.len < 2 {
        print("Usage: swan <input.swan> [-o output.c]");
        exit(2);
    }

    let input_path = args[1];
    var output_path: u8[] = "output.c";
    var i = 2;
    while i < args.len {
        if args[i] == "-o" {
            if i + 1 >= args.len {
                print("Swan error: -o requires an output path");
                exit(2);
            }
            output_path = args[i + 1];
            i += 2;
        } else {
            print("Swan error: unknown argument ", args[i]);
            exit(2);
        }
    }

    var source: u8[>..] = [];
    if !os::read_file(input_path, source) {
        print("Swan error: could not read ", input_path);
        exit(1);
    }

    var generated: u8[>..] = [];
    if !transpile(source, generated) { exit(1); }
    if !os::write_file_atomic(output_path, generated) {
        print("Swan error: could not write ", output_path);
        exit(1);
    }
    print("Wrote C source: ", output_path);
}
