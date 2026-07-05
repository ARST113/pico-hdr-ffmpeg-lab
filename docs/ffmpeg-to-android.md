# FFmpeg To Android HDR Mapping

This is the practical bridge used by the recreated player pipeline.

| FFmpeg value | Meaning | Android `MediaFormat` value |
| --- | --- | --- |
| `AVCOL_PRI_BT2020` | BT.2020 primaries | `COLOR_STANDARD_BT2020` |
| `AVCOL_TRC_SMPTE2084` | PQ / HDR10 | `COLOR_TRANSFER_ST2084` |
| `AVCOL_TRC_ARIB_STD_B67` | HLG | `COLOR_TRANSFER_HLG` |
| `AVCOL_RANGE_MPEG` | limited / TV range | `COLOR_RANGE_LIMITED` |
| `AVCOL_RANGE_JPEG` | full / PC range | `COLOR_RANGE_FULL` |
| `AV_PKT_DATA_MASTERING_DISPLAY_METADATA` | HDR10 mastering display data | encode to `hdr-static-info` when possible |
| `AV_PKT_DATA_CONTENT_LIGHT_LEVEL` | MaxCLL/MaxFALL | encode to `hdr-static-info` when possible |
| `AV_PKT_DATA_DOVI_CONF` | Dolby Vision profile/config | classify as HDR and route to Dolby extension path |

Observed decision shape:

```text
HDR10 = BT.2020 + ST2084
HLG   = BT.2020 + ARIB STD-B67
DOVI  = Dolby Vision profile/config present
P010  = decoder capabilities contain 0x36
```

Android keys to set before `MediaCodec.configure()`:

```text
MediaFormat.KEY_COLOR_STANDARD
MediaFormat.KEY_COLOR_TRANSFER
MediaFormat.KEY_COLOR_RANGE
"hdr-static-info"
```

If a hardware decoder supports P010, prefer it for HDR paths to preserve 10-bit precision. If not, fall back to FFmpeg software decode and upload 10/12-bit YUV planes to textures before shader mapping.

