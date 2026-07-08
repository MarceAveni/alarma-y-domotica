package com.example.alarmaydomotica.ui.main

import androidx.compose.foundation.layout.fillMaxSize
import androidx.compose.foundation.layout.padding
import androidx.compose.material.icons.Icons
import androidx.compose.material.icons.filled.*
import androidx.compose.material3.*
import androidx.compose.runtime.*
import androidx.compose.ui.Modifier
import androidx.compose.ui.res.stringResource
import androidx.compose.ui.unit.sp
import androidx.lifecycle.viewmodel.compose.viewModel
import androidx.navigation3.runtime.NavKey
import com.example.alarmaydomotica.R
import com.example.alarmaydomotica.data.DefaultHomeRepository
import com.example.alarmaydomotica.data.DefaultDataRepository
import androidx.compose.ui.unit.dp
import com.example.alarmaydomotica.theme.DarkBackground
import com.example.alarmaydomotica.theme.DarkSurface
import com.example.alarmaydomotica.theme.BlueNeon
import com.example.alarmaydomotica.theme.OnSurfaceDark
import com.example.alarmaydomotica.ui.screens.*

@Composable
fun MainScreen(
    onItemClick: (NavKey) -> Unit,
    modifier: Modifier = Modifier,
    viewModel: MainScreenViewModel = viewModel { MainScreenViewModel(DefaultDataRepository()) }
) {
    var selectedTab by remember { mutableIntStateOf(0) }

    Scaffold(
        bottomBar = {
            NavigationBar(
                containerColor = DarkSurface,
                tonalElevation = 8.dp
            ) {
                val tabs = listOf(
                    Triple(0, stringResource(R.string.tab_inicio), Icons.Default.Home),
                    Triple(1, stringResource(R.string.tab_clima), Icons.Default.Pool),
                    Triple(2, stringResource(R.string.tab_domotica), Icons.Default.Lightbulb),
                    Triple(3, stringResource(R.string.tab_seguridad), Icons.Default.Security),
                    Triple(4, stringResource(R.string.tab_config), Icons.Default.Settings)
                )

                tabs.forEach { (index, title, icon) ->
                    val isSelected = selectedTab == index
                    NavigationBarItem(
                        selected = isSelected,
                        onClick = { selectedTab = index },
                        icon = {
                            Icon(
                                imageVector = icon,
                                contentDescription = title,
                                tint = if (isSelected) BlueNeon else OnSurfaceDark.copy(alpha = 0.6f)
                            )
                        },
                        label = {
                            Text(
                                text = title,
                                fontSize = 10.sp,
                                color = if (isSelected) BlueNeon else OnSurfaceDark.copy(alpha = 0.6f)
                            )
                        },
                        colors = NavigationBarItemDefaults.colors(
                            indicatorColor = DarkBackground
                        )
                    )
                }
            }
        },
        containerColor = DarkBackground,
        modifier = modifier.fillMaxSize()
    ) { innerPadding ->
        val screenModifier = Modifier.padding(innerPadding)
        
        when (selectedTab) {
            0 -> HomeScreen(repository = viewModel.repository, modifier = screenModifier)
            1 -> ClimatizacionScreen(repository = viewModel.repository, modifier = screenModifier)
            2 -> DomoticaScreen(repository = viewModel.repository, onItemClick = onItemClick, modifier = screenModifier)
            3 -> SeguridadScreen(repository = viewModel.repository, onItemClick = onItemClick, modifier = screenModifier)
            4 -> ConfiguracionScreen(repository = viewModel.repository, onItemClick = onItemClick, modifier = screenModifier)
        }
    }
}
