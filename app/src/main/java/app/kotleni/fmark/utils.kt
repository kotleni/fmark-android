package app.kotleni.fmark

import java.text.DecimalFormat
import java.text.DecimalFormatSymbols

fun Int.toSpacedString(): String = DecimalFormat("#,###", DecimalFormatSymbols().apply {
    groupingSeparator = ' '
}).format(this)