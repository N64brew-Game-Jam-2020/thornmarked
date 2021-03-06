package main

import (
	"bufio"
	"fmt"
	"os"
	"path/filepath"
	"text/template"
)

const headerText = "/* This file is automatically generated */\n"

const tmplType = `#pragma once
#include "base/pak/types.h"
{{- range .Entries}}
#define {{.Ident}} (({{$.Type}}){{"{"}}{{.Index}}{{"}"}})
{{- end}}
enum {
{{- range .Entries}}
    ID_{{.Ident}} = {{.Index}},
{{- end}}
};
`

const tmplPak = `#pragma once
#include "base/pak/types.h"
enum {
    PAK_SIZE = {{.Size}},
{{- range .Sections}}
    PAK_{{.UpperName}}_START = {{.Start}},
    PAK_{{.UpperName}}_COUNT = {{len .Entries}},
{{- end}}
};
{{- range .Sections}}
// Return the object ID for the given asset.
static inline int pak_{{.LowerName}}_object(pak_{{.LowerName}} asset) {
	return PAK_{{.UpperName}}_START + (asset.id - 1) * {{.ObjectsPerAsset}};
}
{{- end}}
`

func writeTemplate(filename string, tmpl *template.Template, d interface{}) error {
	// panic("HELLO: " + strconv.Quote(filename))
	fp, err := os.Create(filename)
	if err != nil {
		return err
	}
	defer fp.Close()
	wr := bufio.NewWriter(fp)
	if _, err := wr.WriteString(headerText); err != nil {
		return err
	}
	if err := tmpl.Execute(wr, d); err != nil {
		return fmt.Errorf("could not execute template: %v", err)
	}
	if err := wr.Flush(); err != nil {
		return err
	}
	return fp.Close()
}

func (mn *manifest) writeCode(dirname, prefix string) error {
	ttype, err := template.New("type.h").Parse(tmplType)
	if err != nil {
		return err
	}
	for _, sec := range mn.Sections {
		filename := filepath.Join(dirname, prefix+sec.dtype.name()+".h")
		if err := writeTemplate(filename, ttype, sec); err != nil {
			return err
		}
	}
	tpak, err := template.New("pak.h").Parse(tmplPak)
	if err != nil {
		return err
	}
	filename := filepath.Join(dirname, prefix+"pak.h")
	return writeTemplate(filename, tpak, mn)
}
