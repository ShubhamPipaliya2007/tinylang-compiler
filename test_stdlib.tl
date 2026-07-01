import "stdlib/String.tl";
import "stdlib/Vec.tl";
import "stdlib/Map.tl";
import "stdlib/File.tl";
import "stdlib/Path.tl";

// ── String ────────────────────────────────────────────────────────────────
print("=== String ===");
print(strLen("hello"));
print(strUpper("hello"));
print(strLower("WORLD"));
print(strTrim("  hello  "));
print(strContains("foobar", "oba"));
print(strStartsWith("foobar", "foo"));
print(strEndsWith("foobar", "bar"));
print(strIndexOf("foobar", "bar"));
print(strReplace("hello world", "world", "TinyLang"));
print(strSub("hello world", 6, 5));
print(strRepeat("ab", 3));
print(strToInt("42"));
print(intToStr(99));
print(strEq("abc", "abc"));
print(strEq("abc", "xyz"));

// ── IntVec ────────────────────────────────────────────────────────────────
print("=== IntVec ===");
IntVec v(8);
v.push(10);
v.push(20);
v.push(30);
v.push(40);
v.push(50);
print(v.len());
print(v.get(0));
print(v.get(4));
print(v.sum());
print(v.min());
print(v.max());
print(v.contains(30));
print(v.contains(99));
print(v.indexOf(40));
v.reverse();
print(v.get(0));

// ── StrVec ────────────────────────────────────────────────────────────────
print("=== StrVec ===");
StrVec sv(4);
sv.push("alpha");
sv.push("beta");
sv.push("gamma");
print(sv.len());
print(sv.get(1));
print(sv.contains("beta"));
print(sv.indexOf("gamma"));
print(sv.join(", "));

// ── IntMap ────────────────────────────────────────────────────────────────
print("=== IntMap ===");
IntMap m(16);
m.put("one", 1);
m.put("two", 2);
m.put("three", 3);
print(m.get("one"));
print(m.get("three"));
print(m.has("two"));
print(m.has("four"));
print(m.len());
m.increment("counter");
m.increment("counter");
m.increment("counter");
print(m.get("counter"));
m.remove("two");
print(m.len());

// ── File ─────────────────────────────────────────────────────────────────
print("=== File ===");
File testFile("/tmp/tl_test.txt");
testFile.write("Hello from TinyLang!\n");
testFile.appendLine("Second line");
print(testFile.exists());
string content = testFile.readAll();
print(content);
testFile.delete();
print(testFile.exists());

// ── Path ─────────────────────────────────────────────────────────────────
print("=== Path ===");
print(pathDir("a/b/c.txt"));
print(pathBase("a/b/c.txt"));
print(pathExt("archive.tar.gz"));
print(pathStem("report.md"));
print(pathJoin("src/lib", "util.tl"));
print(pathIsAbsolute("/usr/bin"));
print(pathIsAbsolute("relative/path"));
print(pathWithExt("doc.txt", ".md"));
